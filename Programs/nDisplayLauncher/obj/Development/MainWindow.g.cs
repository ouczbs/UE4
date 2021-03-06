#pragma checksum "..\..\MainWindow.xaml" "{8829d00f-11b8-4213-878b-770e8597ac16}" "22AA0E062B31E826D77D686A3A2B84CB1C8A6110FDDCDB0115FB77D926B50EE4"
//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

using Microsoft.Windows.Themes;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Forms.Integration;
using System.Windows.Ink;
using System.Windows.Input;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using System.Windows.Media.TextFormatting;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Shell;
using nDisplayLauncher;
using nDisplayLauncher.Cluster;
using nDisplayLauncher.Cluster.Events;
using nDisplayLauncher.UIControl;
using nDisplayLauncher.ValueConversion;


namespace nDisplayLauncher {
    
    
    /// <summary>
    /// MainWindow
    /// </summary>
    public partial class MainWindow : System.Windows.Window, System.Windows.Markup.IComponentConnector {
        
        
        #line 417 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TabControl tabControl;
        
        #line default
        #line hidden
        
        
        #line 418 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TabItem CtrlLauncherTab;
        
        #line default
        #line hidden
        
        
        #line 444 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboRenderApi;
        
        #line default
        #line hidden
        
        
        #line 449 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboRenderMode;
        
        #line default
        #line hidden
        
        
        #line 463 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.CheckBox ctrlCheckAllCores;
        
        #line default
        #line hidden
        
        
        #line 464 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.CheckBox ctrlCheckNoTexStreaming;
        
        #line default
        #line hidden
        
        
        #line 466 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBox ctrlTextCustomArgs;
        
        #line default
        #line hidden
        
        
        #line 468 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBox ctrlTextCustomExecCmds;
        
        #line default
        #line hidden
        
        
        #line 489 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ListBox ctrlListApps;
        
        #line default
        #line hidden
        
        
        #line 496 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnAddApplication;
        
        #line default
        #line hidden
        
        
        #line 497 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnAddEditorGame;
        
        #line default
        #line hidden
        
        
        #line 498 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlDelApplicationBtn;
        
        #line default
        #line hidden
        
        
        #line 509 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboConfigs;
        
        #line default
        #line hidden
        
        
        #line 510 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnAddConfig;
        
        #line default
        #line hidden
        
        
        #line 511 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnDeleteConfig;
        
        #line default
        #line hidden
        
        
        #line 532 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnRun;
        
        #line default
        #line hidden
        
        
        #line 533 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnKill;
        
        #line default
        #line hidden
        
        
        #line 534 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnRestartComputers;
        
        #line default
        #line hidden
        
        
        #line 540 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TabItem CtrlClusterEventsTab;
        
        #line default
        #line hidden
        
        
        #line 554 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventNew;
        
        #line default
        #line hidden
        
        
        #line 555 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventModify;
        
        #line default
        #line hidden
        
        
        #line 556 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventDelete;
        
        #line default
        #line hidden
        
        
        #line 557 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventSend;
        
        #line default
        #line hidden
        
        
        #line 559 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ListView ctrlListClusterEvents;
        
        #line default
        #line hidden
        
        
        #line 578 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TabItem CtrlLogTab;
        
        #line default
        #line hidden
        
        
        #line 580 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.CheckBox ctrlCheckUseCustomLogs;
        
        #line default
        #line hidden
        
        
        #line 607 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogPlugin;
        
        #line default
        #line hidden
        
        
        #line 610 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogEngine;
        
        #line default
        #line hidden
        
        
        #line 613 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogBlueprint;
        
        #line default
        #line hidden
        
        
        #line 616 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogConfig;
        
        #line default
        #line hidden
        
        
        #line 619 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogCluster;
        
        #line default
        #line hidden
        
        
        #line 622 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogGame;
        
        #line default
        #line hidden
        
        
        #line 625 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogGameMode;
        
        #line default
        #line hidden
        
        
        #line 628 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogInput;
        
        #line default
        #line hidden
        
        
        #line 631 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogVRPN;
        
        #line default
        #line hidden
        
        
        #line 634 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogNetwork;
        
        #line default
        #line hidden
        
        
        #line 637 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogNetworkMsg;
        
        #line default
        #line hidden
        
        
        #line 640 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogRender;
        
        #line default
        #line hidden
        
        
        #line 643 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlLogRenderSync;
        
        #line default
        #line hidden
        
        
        #line 646 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityAll;
        
        #line default
        #line hidden
        
        
        #line 647 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityVerbose;
        
        #line default
        #line hidden
        
        
        #line 648 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityLog;
        
        #line default
        #line hidden
        
        
        #line 649 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityDisplay;
        
        #line default
        #line hidden
        
        
        #line 650 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityWarning;
        
        #line default
        #line hidden
        
        
        #line 651 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityError;
        
        #line default
        #line hidden
        
        
        #line 652 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnSetVerbosityFatal;
        
        #line default
        #line hidden
        
        
        #line 666 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBox ctrlTextLog;
        
        #line default
        #line hidden
        
        
        #line 672 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnCopyLog;
        
        #line default
        #line hidden
        
        
        #line 673 "..\..\MainWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnCleanLog;
        
        #line default
        #line hidden
        
        private bool _contentLoaded;
        
        /// <summary>
        /// InitializeComponent
        /// </summary>
        [System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "4.0.0.0")]
        public void InitializeComponent() {
            if (_contentLoaded) {
                return;
            }
            _contentLoaded = true;
            System.Uri resourceLocater = new System.Uri("/nDisplayLauncher;component/mainwindow.xaml", System.UriKind.Relative);
            
            #line 1 "..\..\MainWindow.xaml"
            System.Windows.Application.LoadComponent(this, resourceLocater);
            
            #line default
            #line hidden
        }
        
        [System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "4.0.0.0")]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1811:AvoidUncalledPrivateCode")]
        internal System.Delegate _CreateDelegate(System.Type delegateType, string handler) {
            return System.Delegate.CreateDelegate(delegateType, this, handler);
        }
        
        [System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "4.0.0.0")]
        [System.ComponentModel.EditorBrowsableAttribute(System.ComponentModel.EditorBrowsableState.Never)]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Design", "CA1033:InterfaceMethodsShouldBeCallableByChildTypes")]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Maintainability", "CA1502:AvoidExcessiveComplexity")]
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1800:DoNotCastUnnecessarily")]
        void System.Windows.Markup.IComponentConnector.Connect(int connectionId, object target) {
            switch (connectionId)
            {
            case 1:
            
            #line 18 "..\..\MainWindow.xaml"
            ((nDisplayLauncher.MainWindow)(target)).Loaded += new System.Windows.RoutedEventHandler(this.Window_Loaded);
            
            #line default
            #line hidden
            return;
            case 2:
            this.tabControl = ((System.Windows.Controls.TabControl)(target));
            return;
            case 3:
            this.CtrlLauncherTab = ((System.Windows.Controls.TabItem)(target));
            return;
            case 4:
            this.ctrlComboRenderApi = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 5:
            this.ctrlComboRenderMode = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 6:
            this.ctrlCheckAllCores = ((System.Windows.Controls.CheckBox)(target));
            return;
            case 7:
            this.ctrlCheckNoTexStreaming = ((System.Windows.Controls.CheckBox)(target));
            return;
            case 8:
            this.ctrlTextCustomArgs = ((System.Windows.Controls.TextBox)(target));
            return;
            case 9:
            this.ctrlTextCustomExecCmds = ((System.Windows.Controls.TextBox)(target));
            return;
            case 10:
            this.ctrlListApps = ((System.Windows.Controls.ListBox)(target));
            return;
            case 11:
            this.ctrlBtnAddApplication = ((System.Windows.Controls.Button)(target));
            
            #line 496 "..\..\MainWindow.xaml"
            this.ctrlBtnAddApplication.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnAddApplication_Click);
            
            #line default
            #line hidden
            return;
            case 12:
            this.ctrlBtnAddEditorGame = ((System.Windows.Controls.Button)(target));
            
            #line 497 "..\..\MainWindow.xaml"
            this.ctrlBtnAddEditorGame.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnAddEditorProject_Click);
            
            #line default
            #line hidden
            return;
            case 13:
            this.ctrlDelApplicationBtn = ((System.Windows.Controls.Button)(target));
            
            #line 498 "..\..\MainWindow.xaml"
            this.ctrlDelApplicationBtn.Click += new System.Windows.RoutedEventHandler(this.ctrlDelApplicationBtn_Click);
            
            #line default
            #line hidden
            return;
            case 14:
            this.ctrlComboConfigs = ((System.Windows.Controls.ComboBox)(target));
            
            #line 509 "..\..\MainWindow.xaml"
            this.ctrlComboConfigs.DropDownOpened += new System.EventHandler(this.ctrlComboConfigs_DropDownOpened);
            
            #line default
            #line hidden
            
            #line 509 "..\..\MainWindow.xaml"
            this.ctrlComboConfigs.SelectionChanged += new System.Windows.Controls.SelectionChangedEventHandler(this.ctrlComboConfigs_SelectionChanged);
            
            #line default
            #line hidden
            return;
            case 15:
            this.ctrlBtnAddConfig = ((System.Windows.Controls.Button)(target));
            
            #line 510 "..\..\MainWindow.xaml"
            this.ctrlBtnAddConfig.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnAddConfig_Click);
            
            #line default
            #line hidden
            return;
            case 16:
            this.ctrlBtnDeleteConfig = ((System.Windows.Controls.Button)(target));
            
            #line 511 "..\..\MainWindow.xaml"
            this.ctrlBtnDeleteConfig.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnDeleteConfig_Click);
            
            #line default
            #line hidden
            return;
            case 17:
            this.ctrlBtnRun = ((System.Windows.Controls.Button)(target));
            
            #line 532 "..\..\MainWindow.xaml"
            this.ctrlBtnRun.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnRun_Click);
            
            #line default
            #line hidden
            return;
            case 18:
            this.ctrlBtnKill = ((System.Windows.Controls.Button)(target));
            
            #line 533 "..\..\MainWindow.xaml"
            this.ctrlBtnKill.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnKill_Click);
            
            #line default
            #line hidden
            return;
            case 19:
            this.ctrlBtnRestartComputers = ((System.Windows.Controls.Button)(target));
            
            #line 534 "..\..\MainWindow.xaml"
            this.ctrlBtnRestartComputers.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnRestartComputers_Click);
            
            #line default
            #line hidden
            return;
            case 20:
            this.CtrlClusterEventsTab = ((System.Windows.Controls.TabItem)(target));
            return;
            case 21:
            this.ctrlBtnEventNew = ((System.Windows.Controls.Button)(target));
            
            #line 554 "..\..\MainWindow.xaml"
            this.ctrlBtnEventNew.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventNew_Click);
            
            #line default
            #line hidden
            return;
            case 22:
            this.ctrlBtnEventModify = ((System.Windows.Controls.Button)(target));
            
            #line 555 "..\..\MainWindow.xaml"
            this.ctrlBtnEventModify.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventModify_Click);
            
            #line default
            #line hidden
            return;
            case 23:
            this.ctrlBtnEventDelete = ((System.Windows.Controls.Button)(target));
            
            #line 556 "..\..\MainWindow.xaml"
            this.ctrlBtnEventDelete.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventDelete_Click);
            
            #line default
            #line hidden
            return;
            case 24:
            this.ctrlBtnEventSend = ((System.Windows.Controls.Button)(target));
            
            #line 557 "..\..\MainWindow.xaml"
            this.ctrlBtnEventSend.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventSend_Click);
            
            #line default
            #line hidden
            return;
            case 25:
            this.ctrlListClusterEvents = ((System.Windows.Controls.ListView)(target));
            return;
            case 26:
            this.CtrlLogTab = ((System.Windows.Controls.TabItem)(target));
            return;
            case 27:
            this.ctrlCheckUseCustomLogs = ((System.Windows.Controls.CheckBox)(target));
            return;
            case 28:
            this.ctrlLogPlugin = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 29:
            this.ctrlLogEngine = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 30:
            this.ctrlLogBlueprint = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 31:
            this.ctrlLogConfig = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 32:
            this.ctrlLogCluster = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 33:
            this.ctrlLogGame = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 34:
            this.ctrlLogGameMode = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 35:
            this.ctrlLogInput = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 36:
            this.ctrlLogVRPN = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 37:
            this.ctrlLogNetwork = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 38:
            this.ctrlLogNetworkMsg = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 39:
            this.ctrlLogRender = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 40:
            this.ctrlLogRenderSync = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 41:
            this.ctrlBtnSetVerbosityAll = ((System.Windows.Controls.Button)(target));
            
            #line 646 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityAll.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 42:
            this.ctrlBtnSetVerbosityVerbose = ((System.Windows.Controls.Button)(target));
            
            #line 647 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityVerbose.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 43:
            this.ctrlBtnSetVerbosityLog = ((System.Windows.Controls.Button)(target));
            
            #line 648 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityLog.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 44:
            this.ctrlBtnSetVerbosityDisplay = ((System.Windows.Controls.Button)(target));
            
            #line 649 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityDisplay.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 45:
            this.ctrlBtnSetVerbosityWarning = ((System.Windows.Controls.Button)(target));
            
            #line 650 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityWarning.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 46:
            this.ctrlBtnSetVerbosityError = ((System.Windows.Controls.Button)(target));
            
            #line 651 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityError.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 47:
            this.ctrlBtnSetVerbosityFatal = ((System.Windows.Controls.Button)(target));
            
            #line 652 "..\..\MainWindow.xaml"
            this.ctrlBtnSetVerbosityFatal.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnSetVerbosityAll_Click);
            
            #line default
            #line hidden
            return;
            case 48:
            this.ctrlTextLog = ((System.Windows.Controls.TextBox)(target));
            return;
            case 49:
            this.ctrlBtnCopyLog = ((System.Windows.Controls.Button)(target));
            
            #line 672 "..\..\MainWindow.xaml"
            this.ctrlBtnCopyLog.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnCopyLog_Click);
            
            #line default
            #line hidden
            return;
            case 50:
            this.ctrlBtnCleanLog = ((System.Windows.Controls.Button)(target));
            
            #line 673 "..\..\MainWindow.xaml"
            this.ctrlBtnCleanLog.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnCleanLog_Click);
            
            #line default
            #line hidden
            return;
            }
            this._contentLoaded = true;
        }
    }
}

