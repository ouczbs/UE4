// Copyright Epic Games, Inc. All Rights Reserved.

using EnvDTE;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Security.Policy;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

using Thread = System.Threading.Thread;

namespace UnrealGameSync
{
	public enum IssueBuildOutcome
	{
		Unknown,
		Success,
		Error,
		Warning,
	}

	public class IssueBuildData
	{
		public long Id { get; set; }
		public string Stream { get; set; }
		public int Change { get; set; }
		public string JobName { get; set; }
		public string JobUrl { get; set; }
		public string JobStepName { get; set; }
		public string JobStepUrl { get; set; }
		public string ErrorUrl { get; set; }
		public IssueBuildOutcome Outcome { get; set; }
	}

	public class IssueDiagnosticData
	{
		public long? BuildId { get; set; }
		public string Message { get; set; }
		public string Url { get; set; }
	}

	public class IssueData
	{
		public int Version { get; set; }
		public long Id { get; set; }
		public DateTime CreatedAt { get; set; }
		public DateTime RetrievedAt { get; set; }
		public string Project { get; set; }
		public string Summary { get; set; }
		public string Details { get; set; }
		public string Owner { get; set; }
		public string NominatedBy { get; set; }
		public DateTime? AcknowledgedAt { get; set; }
		public int FixChange { get; set; }
		public DateTime? ResolvedAt { get; set; }
		public bool bNotify { get; set; }
		public bool bIsWarning { get; set; }
		public string BuildUrl { get; set; }
		public List<string> Streams { get;set; }

		HashSet<string> CachedProjects;

		public HashSet<string> Projects
		{
			get
			{
				// HACK to infer project names from streams
				if(CachedProjects == null)
				{
					HashSet<string> NewProjects = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
					if (!String.IsNullOrEmpty(Project))
					{
						NewProjects.Add(Project);
					}
					if (Streams != null)
					{
						foreach (string Stream in Streams)
						{
							Match Match = Regex.Match(Stream, "^//([^/]+)/");
							if (Match.Success)
							{
								string Project = Match.Groups[1].Value;
								if (Project.StartsWith("UE", StringComparison.OrdinalIgnoreCase))
								{
									Project = "UE" + Project.Substring(2);
								}
								else if (Char.IsLower(Project[0]))
								{
									Project = Char.ToUpper(Project[0], CultureInfo.InvariantCulture) + Project.Substring(1);
								}
								NewProjects.Add(Project);
							}
						}
					}
					if (NewProjects.Count == 0)
					{
						NewProjects.Add("Default");
					}
					CachedProjects = NewProjects;
				}
				return CachedProjects;
			}
		}
	}

	public class IssueUpdateData
	{
		public long Id { get; set; }
		public string Owner { get; set; }
		public string NominatedBy { get; set; }
		public bool? Acknowledged { get; set; }
		public int? FixChange { get; set; }
		public bool? Resolved { get; set; }
	}

	[Flags]
	public enum IssueAlertReason
	{
		Normal = 1,
		Owner = 2,
		UnassignedTimer = 4,
		UnacknowledgedTimer = 8,
		UnresolvedTimer = 16,
	}

	class IssueMonitor : IDisposable
	{
		public readonly string ApiUrl;
		public readonly string UserName;
		int RefCount = 1;
		Thread WorkerThread;
		BoundedLogWriter LogWriter;
		bool bDisposing;
		AutoResetEvent RefreshEvent;
		int UpdateIntervalMs;
		List<long> TrackingIssueIds = new List<long>();
		List<IssueData> Issues = new List<IssueData>();
		object LockObject = new object();
		List<IssueUpdateData> PendingUpdates = new List<IssueUpdateData>();

		public event Action OnIssuesChanged;

		// Only used by MainWindow, but easier to just store here
		public Dictionary<long, IssueAlertReason> IssueIdToAlertReason = new Dictionary<long, IssueAlertReason>();

		public IssueMonitor(string ApiUrl, string UserName, TimeSpan UpdateInterval, string LogFileName)
		{
			this.ApiUrl = ApiUrl;
			this.UserName = UserName;
			this.UpdateIntervalMs = (int)UpdateInterval.TotalMilliseconds;

			LogWriter = new BoundedLogWriter(LogFileName);
			if(ApiUrl == null)
			{
				LastStatusMessage = "Database functionality disabled due to empty ApiUrl.";
			}
			else
			{
				LogWriter.WriteLine("Using connection string: {0}", this.ApiUrl);
			}

			RefreshEvent = new AutoResetEvent(false);
		}

		public string LastStatusMessage
		{
			get;
			private set;
		}

		public List<IssueData> GetIssues()
		{
			return Issues;
		}

		public TimeSpan GetUpdateInterval()
		{
			return TimeSpan.FromMilliseconds(UpdateIntervalMs);
		}

		public void SetUpdateInterval(TimeSpan UpdateInterval)
		{
			UpdateIntervalMs = (int)UpdateInterval.TotalMilliseconds;
			RefreshEvent.Set();
		}

		public void StartTracking(long IssueId)
		{
			lock(LockObject)
			{
				TrackingIssueIds.Add(IssueId);
			}
			RefreshEvent.Set();
		}

		public void StopTracking(long IssueId)
		{
			lock(LockObject)
			{
				TrackingIssueIds.RemoveAt(TrackingIssueIds.IndexOf(IssueId));
			}
		}

		public bool HasPendingUpdate()
		{
			return PendingUpdates.Count > 0;
		}

		public void PostUpdate(IssueUpdateData Update)
		{
			bool bUpdatedIssues;
			lock(LockObject)
			{
				PendingUpdates.Add(Update);
				bUpdatedIssues = ApplyPendingUpdate(Issues, Update);
			}

			RefreshEvent.Set();

			if(bUpdatedIssues)
			{
				OnIssuesChanged?.Invoke();
			}
		}

		static bool ApplyPendingUpdate(List<IssueData> Issues, IssueUpdateData Update)
		{
			bool bUpdated = false;
			for(int Idx = 0; Idx < Issues.Count; Idx++)
			{
				IssueData Issue = Issues[Idx];
				if(Update.Id == Issue.Id)
				{
					if(Update.Owner != null && Update.Owner != Issue.Owner)
					{
						Issue.Owner = Update.Owner;
						bUpdated = true;
					}
					if(Update.NominatedBy != null && Update.NominatedBy != Issue.NominatedBy)
					{
						Issue.NominatedBy = Update.NominatedBy;
						bUpdated = true;
					}
					if(Update.Acknowledged.HasValue && Update.Acknowledged.Value != Issue.AcknowledgedAt.HasValue)
					{
						Issue.AcknowledgedAt = Update.Acknowledged.Value? (DateTime?)DateTime.UtcNow : null;
						bUpdated = true;
					}
					if(Update.FixChange.HasValue)
					{
						Issue.FixChange = Update.FixChange.Value;
						if(Issue.FixChange != 0)
						{
							Issues.RemoveAt(Idx);
						}
						bUpdated = true;
					}
					break;
				}
			}
			return bUpdated;
		}

		public void Start()
		{
			if(ApiUrl != null)
			{
				WorkerThread = new Thread(() => PollForUpdates());
				WorkerThread.Start();
			}
		}

		public void AddRef()
		{
			if(RefCount == 0)
			{
				throw new Exception("Invalid reference count for IssueMonitor (zero)");
			}
			RefCount++;
		}

		public void Release()
		{
			RefCount--;
			if(RefCount < 0)
			{
				throw new Exception("Invalid reference count for IssueMonitor (ltz)");
			}
			if(RefCount == 0)
			{
				DisposeInternal();
			}
		}

		void IDisposable.Dispose()
		{
			DisposeInternal();
		}

		void DisposeInternal()
		{
			bDisposing = true;
			if(WorkerThread != null)
			{
				RefreshEvent.Set();
				if(!WorkerThread.Join(100))
				{
					WorkerThread.Interrupt();
					WorkerThread.Join();
				}
				WorkerThread = null;
			}
			if(LogWriter != null)
			{
				LogWriter.Dispose();
				LogWriter = null;
			}
		}

		void PollForUpdates()
		{
			while (!bDisposing)
			{
				// Check if there's any pending update
				IssueUpdateData PendingUpdate;
				lock (LockObject)
				{
					if (PendingUpdates.Count > 0)
					{
						PendingUpdate = PendingUpdates[0];
					}
					else
					{
						PendingUpdate = null;
					}
				}

				// If we have an update, try to post it to the backend and check for another
				if (PendingUpdate != null)
				{
					if (SendUpdate(PendingUpdate))
					{
						lock (LockObject) { PendingUpdates.RemoveAt(0); }
					}
					else
					{
						RefreshEvent.WaitOne(5 * 1000);
					}
					continue;
				}

				// Read all the current issues
				ReadCurrentIssues();

				// Wait for something else to do
				RefreshEvent.WaitOne(UpdateIntervalMs);
			}
		}

		bool SendUpdate(IssueUpdateData Update)
		{
			try
			{
				RESTApi.PUT<IssueUpdateData>(ApiUrl, String.Format("issues/{0}", Update.Id), Update);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				LastStatusMessage = String.Format("Failed to send update: ({0})", Ex.ToString());
				return false;
			}
		}

		bool ReadCurrentIssues()
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine();
				LogWriter.WriteLine("Polling for notifications at {0}...", DateTime.Now.ToString());

				// Get the initial number of issues. We won't post updates if this stays at zero.
				int InitialNumIssues = Issues.Count;

				// Fetch the new issues
				List<IssueData> NewIssues = RESTApi.GET<List<IssueData>>(ApiUrl, String.Format("issues?user={0}", UserName));

				// Check if we're tracking a particular issue. If so, we want updates even when it's resolved.
				long[] LocalTrackingIssueIds;
				lock(LockObject)
				{
					LocalTrackingIssueIds = TrackingIssueIds.Distinct().ToArray();
				}
				foreach(long LocalTrackingIssueId in LocalTrackingIssueIds)
				{
					if(!NewIssues.Any(x => x.Id == LocalTrackingIssueId))
					{
						try
						{
							IssueData Issue = RESTApi.GET<IssueData>(ApiUrl, String.Format("issues/{0}", LocalTrackingIssueId));
							if(Issue != null)
							{
								NewIssues.Add(Issue);
							}
						}
						catch(Exception Ex)
						{
							LogWriter.WriteException(Ex, "Exception while fetching tracked issue");
						}
					}
				}

				// Update all the builds for each issue
				foreach (IssueData NewIssue in NewIssues)
				{
					if (NewIssue.Version == 0)
					{
						List<IssueBuildData> Builds = RESTApi.GET<List<IssueBuildData>>(ApiUrl, String.Format("issues/{0}/builds", NewIssue.Id));
						if (Builds != null && Builds.Count > 0)
						{
							NewIssue.bIsWarning = !Builds.Any(x => x.Outcome != IssueBuildOutcome.Warning);

							IssueBuildData LastBuild = Builds.OrderByDescending(x => x.Change).FirstOrDefault();
							if (LastBuild != null && !String.IsNullOrEmpty(LastBuild.ErrorUrl))
							{
								NewIssue.BuildUrl = LastBuild.ErrorUrl;
							}
						}
					}
				}

				// Apply any pending updates to this issue list, and update it
				lock (LockObject)
				{
					foreach(IssueUpdateData PendingUpdate in PendingUpdates)
					{
						ApplyPendingUpdate(NewIssues, PendingUpdate);
					}
					Issues = NewIssues;
				}

				// Update the main thread
				if(InitialNumIssues > 0 || Issues.Count > 0)
				{
					if(OnIssuesChanged != null)
					{
						OnIssuesChanged();
					}
				}

				// Update the stats
				LastStatusMessage = String.Format("Last update took {0}ms", Timer.ElapsedMilliseconds);
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				LastStatusMessage = String.Format("Last update failed: ({0})", Ex.ToString());
				return false;
			}
		}
	}
}
