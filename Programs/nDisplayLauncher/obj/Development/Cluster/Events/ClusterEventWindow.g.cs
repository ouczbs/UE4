﻿#pragma checksum "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml" "{8829d00f-11b8-4213-878b-770e8597ac16}" "71F5252BCCAD186C913A99DDD18BE7A93803ED51EBF69F25321C5E00FF507BB2"
//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

using System;
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
using nDisplayLauncher.Cluster.Events;
using nDisplayLauncher.UIControl;


namespace nDisplayLauncher.Cluster.Events {
    
    
    /// <summary>
    /// ClusterEventWindow
    /// </summary>
    public partial class ClusterEventWindow : System.Windows.Window, System.Windows.Markup.IComponentConnector {
        
        
        #line 425 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboEventCategory;
        
        #line default
        #line hidden
        
        
        #line 429 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboEventType;
        
        #line default
        #line hidden
        
        
        #line 433 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ComboBox ctrlComboEventName;
        
        #line default
        #line hidden
        
        
        #line 439 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBox ctrlTextEventArgument;
        
        #line default
        #line hidden
        
        
        #line 442 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.TextBox ctrlTextEventValue;
        
        #line default
        #line hidden
        
        
        #line 445 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventEditorArgDel;
        
        #line default
        #line hidden
        
        
        #line 446 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventEditorArgAdd;
        
        #line default
        #line hidden
        
        
        #line 449 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.ListView ctrlListClusterEventArgs;
        
        #line default
        #line hidden
        
        
        #line 471 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventEditorCancel;
        
        #line default
        #line hidden
        
        
        #line 472 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
        [System.Diagnostics.CodeAnalysis.SuppressMessageAttribute("Microsoft.Performance", "CA1823:AvoidUnusedPrivateFields")]
        internal System.Windows.Controls.Button ctrlBtnEventEditorApply;
        
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
            System.Uri resourceLocater = new System.Uri("/nDisplayLauncher;component/cluster/events/clustereventwindow.xaml", System.UriKind.Relative);
            
            #line 1 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            System.Windows.Application.LoadComponent(this, resourceLocater);
            
            #line default
            #line hidden
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
            
            #line 10 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            ((nDisplayLauncher.Cluster.Events.ClusterEventWindow)(target)).Initialized += new System.EventHandler(this.Window_Initialized);
            
            #line default
            #line hidden
            return;
            case 2:
            this.ctrlComboEventCategory = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 3:
            this.ctrlComboEventType = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 4:
            this.ctrlComboEventName = ((System.Windows.Controls.ComboBox)(target));
            return;
            case 5:
            this.ctrlTextEventArgument = ((System.Windows.Controls.TextBox)(target));
            return;
            case 6:
            this.ctrlTextEventValue = ((System.Windows.Controls.TextBox)(target));
            return;
            case 7:
            this.ctrlBtnEventEditorArgDel = ((System.Windows.Controls.Button)(target));
            
            #line 445 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            this.ctrlBtnEventEditorArgDel.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventEditorArgDel_Click);
            
            #line default
            #line hidden
            return;
            case 8:
            this.ctrlBtnEventEditorArgAdd = ((System.Windows.Controls.Button)(target));
            
            #line 446 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            this.ctrlBtnEventEditorArgAdd.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventEditorArgAdd_Click);
            
            #line default
            #line hidden
            return;
            case 9:
            this.ctrlListClusterEventArgs = ((System.Windows.Controls.ListView)(target));
            return;
            case 10:
            this.ctrlBtnEventEditorCancel = ((System.Windows.Controls.Button)(target));
            
            #line 471 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            this.ctrlBtnEventEditorCancel.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventEditorCancel_Click);
            
            #line default
            #line hidden
            return;
            case 11:
            this.ctrlBtnEventEditorApply = ((System.Windows.Controls.Button)(target));
            
            #line 472 "..\..\..\..\Cluster\Events\ClusterEventWindow.xaml"
            this.ctrlBtnEventEditorApply.Click += new System.Windows.RoutedEventHandler(this.ctrlBtnEventEditorApply_Click);
            
            #line default
            #line hidden
            return;
            }
            this._contentLoaded = true;
        }
    }
}

