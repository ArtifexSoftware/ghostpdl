/* Copyright (C) 2020-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

using System;
using System.Collections.Generic;
using Gtk;
using GhostMono;
using System.Diagnostics;

namespace gs_mono_example
{
    static class Constants
    {
        public const double SCALE_THUMB = 0.1;
        public const int DEFAULT_GS_RES = 300;
        public const int ZOOM_MAX = 4;
        public const double ZOOM_MIN = .25;
    }

    public enum NotifyType_t
    {
        MESS_STATUS,
        MESS_ERROR
    };

    public enum Page_Content_t
    {
        FULL_RESOLUTION = 0,
        THUMBNAIL,
        OLD_RESOLUTION,
        LOW_RESOLUTION,
        NOTSET,
        BLANK
    };

    public struct pagesizes_t
    {
        public double width;
        public double height;
    }

    public partial class MainWindow : Gtk.Window
    {
        GSMONO m_ghostscript;
        bool m_file_open;
        String m_currfile;
        String m_extension;
        List<TempFile> m_tempfiles;
        String m_origfile;
        int m_currpage;
        gsOutput m_gsoutput;
        public int m_numpages;
        private static Pages m_docPages;
        private static double m_doczoom;
        public List<pagesizes_t> m_page_sizes;
        bool m_init_done;
        bool m_busy_render;
        bool m_firstime;
        bool m_aa;
        bool m_aa_change;
        List<int> m_page_scroll_pos;
        Gtk.ProgressBar m_GtkProgressBar;
        Label m_GtkProgressLabel;
        HBox m_GtkProgressBox;
        Gtk.TreeView m_GtkTreeThumb;
        Gtk.TreeView m_GtkTreeMain;
        Gtk.VBox m_GtkvBoxMain;
        Label m_GtkpageTotal;
        Entry m_GtkpageEntry;
        Gtk.ListStore m_GtkimageStoreThumb;
        Gtk.ListStore m_GtkimageStoreMain;
        Gtk.ScrolledWindow m_GtkthumbScroll;
        Gtk.ScrolledWindow m_GtkmainScroll;
        Gtk.Entry m_GtkzoomEntry;
        Gtk.CheckButton m_GtkaaCheck;
        Gtk.Button m_GtkZoomPlus;
        Gtk.Button m_GtkZoomMinus;
        String m_zoom_txt;
        String m_page_txt;
        bool m_ignore_scroll_change;

        void ShowMessage(NotifyType_t type, string message)
        {
            Gtk.MessageType mess;

            if (type == NotifyType_t.MESS_ERROR)
            {
                mess = MessageType.Error;
            }
            else
            {
                mess = MessageType.Warning;
            }

            /* Dispatch on UI thread */
            Gtk.Application.Invoke(delegate
            {
                MessageDialog md = new MessageDialog(null,
                                   DialogFlags.Modal,
                                   mess,
                                   ButtonsType.Ok,
                                   message);
                md.SetPosition(WindowPosition.Center);
                md.ShowAll();
                md.Run();
                md.Destroy();
            });
        }

        public MainWindow() : base(Gtk.WindowType.Toplevel)
        {
            /* Set up ghostscript calls for progress update */
            m_ghostscript = new GSMONO();
            m_ghostscript.ProgressCallBack += new GSMONO.Progress(gsProgress);
            m_ghostscript.StdIOCallBack += new GSMONO.StdIO(gsIO);
            m_ghostscript.DLLProblemCallBack += new GSMONO.DLLProblem(gsDLL);

            DeleteEvent += delegate { Application.Quit(); };

            m_currpage = 0;
            m_gsoutput = new gsOutput();
            m_gsoutput.Activate();
            m_tempfiles = new List<TempFile>();
            m_thumbnails = new List<DocPage>();
            m_docPages = new Pages();
            m_page_sizes = new List<pagesizes_t>();
            m_page_scroll_pos = new List<int>();
            m_file_open = false;
            m_doczoom = 1.0;
            m_init_done = false;
            m_busy_render = true;
            m_firstime = true;
            m_aa = true;
            m_aa_change = false;
            m_zoom_txt = "100";
            m_page_txt = "1";
            m_ignore_scroll_change = false;

            /* Set up Vbox in main window */
            this.SetDefaultSize(500, 700);
            this.Title = "GhostPDL Mono GTK Demo";
            m_GtkvBoxMain = new VBox(false, 0);
            this.Add(m_GtkvBoxMain);

            /* Add Menu Bar to vBox */
            Gtk.MenuBar menu_bar = new MenuBar();
            Menu filemenu = new Menu();
            MenuItem file = new MenuItem("File");
            file.Submenu = filemenu;

            AccelGroup agr = new AccelGroup();
            AddAccelGroup(agr);

            ImageMenuItem openi = new ImageMenuItem(Stock.Open, agr);
            openi.AddAccelerator("activate", agr, new AccelKey(
                Gdk.Key.o, Gdk.ModifierType.ControlMask, AccelFlags.Visible));
            openi.Activated += OnOpen;
            filemenu.Append(openi);

            ImageMenuItem closei = new ImageMenuItem(Stock.Close, agr);
            closei.AddAccelerator("activate", agr, new AccelKey(
                Gdk.Key.w, Gdk.ModifierType.ControlMask, AccelFlags.Visible));
            closei.Activated += OnClose;
            filemenu.Append(closei);

            MenuItem messagesi = new MenuItem("Show Messages");
            messagesi.Activated += OnShowMessages;
            filemenu.Append(messagesi);

            MenuItem about = new MenuItem("About");
            about.Activated += OnAboutClicked;
            filemenu.Append(about);

            SeparatorMenuItem sep = new SeparatorMenuItem();
            filemenu.Append(sep);

            ImageMenuItem quiti = new ImageMenuItem(Stock.Quit, agr);
            quiti.AddAccelerator("activate", agr, new AccelKey(
                Gdk.Key.q, Gdk.ModifierType.ControlMask, AccelFlags.Visible));
            filemenu.Append(quiti);
            quiti.Activated += OnQuit;

            menu_bar.Append(file);

            m_GtkvBoxMain.PackStart(menu_bar, false, false, 0);

            /* Add a hbox with the page information, zoom control, and aa to vbox */
            HBox pageBox = new HBox(false, 0);
            m_GtkpageEntry = new Entry();
            m_GtkpageEntry.Activated += PageChanged;
            m_GtkpageEntry.WidthChars = 4;
            m_GtkpageTotal = new Label("/0");
            pageBox.PackStart(m_GtkpageEntry, false, false, 0);
            pageBox.PackStart(m_GtkpageTotal, false, false, 0);

            HBox zoomBox = new HBox(false, 0);
            m_GtkZoomPlus = new Button();
            m_GtkZoomPlus.Label = "+";
            m_GtkZoomPlus.Clicked += ZoomIn;
            m_GtkZoomMinus = new Button();
            m_GtkZoomMinus.Label = "–";
            m_GtkZoomMinus.Clicked += ZoomOut;
            m_GtkzoomEntry = new Entry();
            m_GtkzoomEntry.WidthChars = 3;
            m_GtkzoomEntry.Activated += ZoomChanged;
            Label precentLabel = new Label("%");
            zoomBox.PackStart(m_GtkZoomPlus, false, false, 0);
            zoomBox.PackStart(m_GtkZoomMinus, false, false, 0);
            zoomBox.PackStart(m_GtkzoomEntry, false, false, 0);
            zoomBox.PackStart(precentLabel, false, false, 0);

            HBox hBoxControls = new HBox(false, 0);
            m_GtkaaCheck = new CheckButton("Enable Antialias:");
            m_GtkaaCheck.Active = true;
            m_GtkaaCheck.Clicked += AaCheck_Clicked;
            hBoxControls.PackStart(pageBox, false, false, 0);
            hBoxControls.PackStart(zoomBox, false, false, 20);
            hBoxControls.PackStart(m_GtkaaCheck, false, false, 0);

            m_GtkvBoxMain.PackStart(hBoxControls, false, false, 0);

            /* Tree view containing thumbnail and main images */
            HBox hBoxPages = new HBox(false, 0);

            /* Must be scrollable */
            m_GtkthumbScroll = new ScrolledWindow();
            m_GtkthumbScroll.BorderWidth = 5;
            m_GtkthumbScroll.ShadowType = ShadowType.In;

            m_GtkmainScroll = new ScrolledWindow();
            m_GtkmainScroll.BorderWidth = 5;
            m_GtkmainScroll.ShadowType = ShadowType.In;
            m_GtkmainScroll.Vadjustment.ValueChanged += Vadjustment_Changed;

            m_GtkTreeThumb = new Gtk.TreeView();
            m_GtkTreeThumb.HeadersVisible = false;
            m_GtkimageStoreThumb = new Gtk.ListStore(typeof(Gdk.Pixbuf));
            m_GtkTreeThumb.AppendColumn("Thumb", new Gtk.CellRendererPixbuf(), "pixbuf", 0);
            m_GtkTreeThumb.Style.YThickness = 100;
            m_GtkthumbScroll.Add(m_GtkTreeThumb);
            m_GtkTreeThumb.RowActivated += M_GtkTreeThumb_RowActivated;

            m_GtkTreeMain = new Gtk.TreeView();
            m_GtkTreeMain.HeadersVisible = false;
            m_GtkTreeMain.RulesHint = false;

            m_GtkimageStoreMain = new Gtk.ListStore(typeof(Gdk.Pixbuf));
            m_GtkTreeMain.AppendColumn("Main", new Gtk.CellRendererPixbuf(), "pixbuf", 0);
            m_GtkmainScroll.Add(m_GtkTreeMain);

            // Separate with gridlines
            m_GtkTreeMain.EnableGridLines = TreeViewGridLines.Horizontal;

            //To disable selections, set the selection mode to None:
            m_GtkTreeMain.Selection.Mode = SelectionMode.None;

            hBoxPages.PackStart(m_GtkthumbScroll, false, false, 0);
            hBoxPages.PackStart(m_GtkmainScroll, true, true, 0);

            m_GtkTreeThumb.Model = m_GtkimageStoreThumb;
            m_GtkTreeMain.Model = m_GtkimageStoreMain;

            m_GtkvBoxMain.PackStart(hBoxPages, true, true, 0);

            /* For case of opening another file */
            string[] arguments = Environment.GetCommandLineArgs();
            if (arguments.Length > 1)
            {
                m_currfile = arguments[1];
                ProcessFile();
            }
        }

        void Vadjustment_Changed(object sender, EventArgs e)
        {
            if (!m_init_done)
            {
                return;
            }

            if (m_ignore_scroll_change)
            {
                m_ignore_scroll_change = false;
                return;
            }

            Gtk.Adjustment zz = sender as Gtk.Adjustment;
            double val = zz.Value;
            int page = 1;

            for (int k = 0; k < m_numpages; k++)
            {
                if (val <= m_page_scroll_pos[k])
                {
                    m_GtkpageEntry.Text = page.ToString();
                    m_page_txt = m_GtkpageEntry.Text;
                    return;
                }
                page += 1;
            }
            m_GtkpageEntry.Text = m_numpages.ToString();
            m_page_txt = m_GtkpageEntry.Text;
        }

        void OnAboutClicked(object sender, EventArgs args)
        {
            AboutDialog about = new AboutDialog();
            about.ProgramName = "GhostPDL Gtk# Viewer";
            about.Version = "0.1";
            about.Copyright = "(c) Artifex Software";
            about.Comments = @"A demo of GhostPDL API with C# Mono";
            about.Website = "http://www.artifex.com";
            about.Run();
            about.Destroy();
        }

        /* C# Insanity to get the index of the selected item */
        void M_GtkTreeThumb_RowActivated(object o, RowActivatedArgs args)
        {
            TreeModel treeModel;
            TreeSelection my_selected_row = m_GtkTreeThumb.Selection;

            var TreePath = my_selected_row.GetSelectedRows(out treeModel);
            var item = TreePath.GetValue(0);
            var result = item.ToString();
            int index = System.Convert.ToInt32(result);

            ScrollMainTo(index);                  
        }

        void ScrollMainTo(int index)
        {
            m_ignore_scroll_change = true;

            if (index < 0 || index > m_numpages - 1)
                return;

            if (index == 0)
                m_GtkmainScroll.Vadjustment.Value = 0;
            else
                m_GtkmainScroll.Vadjustment.Value = m_page_scroll_pos[index-1];

            m_currpage = index + 1;
            m_GtkpageEntry.Text = m_currpage.ToString();
            m_page_txt = m_GtkpageEntry.Text;
        }

        void AddProgressBar(String text)
        {
            /* Progress bar */
            m_GtkProgressBox = new HBox(false, 0);
            m_GtkProgressBar = new ProgressBar();
            m_GtkProgressBar.Orientation = ProgressBarOrientation.LeftToRight;
            m_GtkProgressBox.PackStart(m_GtkProgressBar, true, true, 0);
            m_GtkProgressLabel = new Label(text);
            m_GtkProgressBox.PackStart(m_GtkProgressLabel, false, false, 0);
            m_GtkvBoxMain.PackStart(m_GtkProgressBox, false, false, 0);
            m_GtkProgressBar.Fraction = 0.0;
            this.ShowAll();
        }

        void RemoveProgressBar()
        {
            m_GtkvBoxMain.Remove(m_GtkProgressBox);
            this.ShowAll();
        }

        void AaCheck_Clicked(object sender, EventArgs e)
        {
            m_aa = !m_aa;
            m_aa_change = true;
            if (m_init_done && !m_busy_render)
                RenderMainAll();
        }

        private void OnQuit(object sender, EventArgs e)
        {
            Application.Quit();
        }

        private void OnShowMessages(object sender, EventArgs e)
        {
            m_gsoutput.Show();
        }

        private void OnClose(object sender, EventArgs e)
        {
            if (m_init_done)
                CleanUp(); 
        }

        private void gsIO(String mess, int len)
        {
            Gtk.TextBuffer buffer = m_gsoutput.m_textView.Buffer;
            Gtk.TextIter ti = buffer.EndIter;

            var part = mess.Substring(0, len);
            buffer.Insert(ref ti, part);
        }

        private void gsDLL(String mess)
        {
            ShowMessage(NotifyType_t.MESS_STATUS, mess);
        }

        private void gsThumbRendered(int width, int height, int raster,
                                    IntPtr data, gsParamState_t state)
        {
            ThumbPageCallback(width, height, raster, state.zoom, state.currpage, data);
        }

        private void gsPageRendered(int width, int height, int raster,
                                    IntPtr data, gsParamState_t state)
        {
            MainPageCallback(width, height, raster, state.zoom, state.currpage, data);
        }

        private void gsProgress(gsThreadCallBack info)
        {
            if (info.Completed)
            {
                switch (info.Params.task)
                {
                    case GS_Task_t.PS_DISTILL:
                        m_GtkProgressBar.Fraction = 1.0;
                        RemoveProgressBar();
                        break;

                    case GS_Task_t.SAVE_RESULT:
                        break;

                    case GS_Task_t.DISPLAY_DEV_THUMBS_NON_PDF:
                    case GS_Task_t.DISPLAY_DEV_THUMBS_PDF:
                        ThumbsDone();
                        break;

                    case GS_Task_t.DISPLAY_DEV_PDF:
                    case GS_Task_t.DISPLAY_DEV_NON_PDF:
                        RenderingDone();
                        break;
                }

                if (info.Params.result == GS_Result_t.gsFAILED)
                {
                    switch (info.Params.task)
                    {
                        case GS_Task_t.CREATE_XPS:
                            ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to create XPS");
                            break;

                        case GS_Task_t.PS_DISTILL:
                            ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to distill PS");
                            break;

                        case GS_Task_t.SAVE_RESULT:
                            ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to convert document");
                            break;

                        default:
                            ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed.");
                            break;
                    }
                    return;
                }
                GSResult(info.Params);
            }
            else
            {
                switch (info.Params.task)
                {
                    case GS_Task_t.CREATE_XPS:
                    case GS_Task_t.PS_DISTILL:
                        m_GtkProgressBar.Fraction = (double)(info.Progress) / 100.0;
                        break;

                    case GS_Task_t.SAVE_RESULT:
                        break;
                }
            }
        }

        /* GS Result*/
        public void GSResult(gsParamState_t gs_result)
        {
            TempFile tempfile = null;

            if (gs_result.outputfile != null)
                 tempfile = new TempFile(gs_result.outputfile);

            if (gs_result.result == GS_Result_t.gsCANCELLED)
            {
                if (tempfile != null)
                {
                    try
                    {
                        tempfile.DeleteFile();
                    }
                    catch
                    {
                        ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
                    }
                }
                return;
            }
            if (gs_result.result == GS_Result_t.gsFAILED)
            {
                ShowMessage(NotifyType_t.MESS_STATUS, "GS Failed Conversion");
                if (tempfile != null)
                {
                    try
                    {
                        tempfile.DeleteFile();
                    }
                    catch
                    {
                        ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
                    }
                }
                return;
            }

            switch (gs_result.task)
            {
                case GS_Task_t.PS_DISTILL:
                    m_origfile = gs_result.inputfile;

                    Gtk.FileChooserDialog dialog = new Gtk.FileChooserDialog("Save Document",
                                                    (Gtk.Window)this.Toplevel,
                                                     Gtk.FileChooserAction.Save);                                                   
                    dialog.LocalOnly = true;
                    dialog.AddButton(Gtk.Stock.Cancel, Gtk.ResponseType.Cancel);
                    dialog.AddButton(Gtk.Stock.Save, Gtk.ResponseType.Yes);
                    dialog.SetFilename(System.IO.Path.GetFileNameWithoutExtension(m_origfile) + ".pdf");             

                    Gtk.FileFilter filter = new Gtk.FileFilter();
                    filter.Name = "doc/pdf";
                    filter.AddMimeType("application/pdf");
                    filter.AddPattern("*.pdf");
                    dialog.Filter = filter;

                    int response = dialog.Run();
                    if (response == (int)Gtk.ResponseType.Yes)
                    {
                        m_currfile = dialog.Filename;
                        try
                        {
                            if (System.IO.File.Exists(m_currfile))
                            {
                                System.IO.File.Delete(m_currfile);
                            }

                            System.IO.File.Exists(tempfile.Filename);
                            System.IO.File.Copy(tempfile.Filename, dialog.Filename);
                        }
                        catch (Exception except)
                        {
                            ShowMessage(NotifyType_t.MESS_ERROR, "Exception Saving Distilled Result:" + except.Message);
                        }
                    }

                    dialog.Destroy();
                    tempfile.DeleteFile();
                    break;
            }                        
        }

        protected void OnDeleteEvent(object sender, DeleteEventArgs a)
        {
            Application.Quit();
            a.RetVal = true;
        }

        protected void OnOpen(object sender, EventArgs e)
        {
            Gtk.FileChooserDialog dialog = new Gtk.FileChooserDialog("Open Document",
                    (Gtk.Window)this.Toplevel,
                    Gtk.FileChooserAction.Open);

            dialog.AddButton(Gtk.Stock.Cancel, Gtk.ResponseType.Cancel);
            dialog.AddButton(Gtk.Stock.Open, Gtk.ResponseType.Accept);

            dialog.DefaultResponse = Gtk.ResponseType.Cancel;
            dialog.LocalOnly = true;

            Gtk.FileFilter filter = new Gtk.FileFilter();
            filter.Name = "doc/pdf";
            filter.AddMimeType("application/pdf");
            filter.AddMimeType("application/ps");
            filter.AddMimeType("application/eps");
            filter.AddMimeType("application/pcl");
            filter.AddMimeType("application/xps");
            filter.AddMimeType("application/oxps");
            filter.AddPattern("*.pdf");
            filter.AddPattern("*.ps");
            filter.AddPattern("*.eps");
            filter.AddPattern("*.bin");
            filter.AddPattern("*.xps");
            filter.AddPattern("*.oxps");
            dialog.Filter = filter;
            int response = dialog.Run();

            if (response != (int)Gtk.ResponseType.Accept)
            {
                dialog.Destroy();
                return;
            }

            if (m_file_open)
            {
                /* launch a new process */
                string path = System.Reflection.Assembly.GetExecutingAssembly().Location;

                try
                {
                    String name = dialog.Filename;
                    Process.Start(path, name);
                }
                catch (Exception except)
                {
                    Console.WriteLine("Problem opening file: " + except.Message);
                }
                dialog.Destroy();
                return;
            }

            m_currfile = dialog.Filename;
            dialog.Destroy();
            ProcessFile();
        }

        public void ProcessFile()
        {
            m_extension = m_currfile.Split('.')[1];
            int result;
            if (!(m_extension.ToUpper() == "PDF" || m_extension.ToUpper() == "pdf"))
            {
                MessageDialog md = new MessageDialog(this,
                    DialogFlags.DestroyWithParent, MessageType.Question,
                    ButtonsType.YesNo, "Would you like to Distill this file ?");

                result = md.Run();
                md.Destroy();
                if ((ResponseType)result == ResponseType.Yes)
                {
                    AddProgressBar("Distilling");
                    if (m_ghostscript.DistillPS(m_currfile, Constants.DEFAULT_GS_RES) == gsStatus.GS_BUSY)
                    {
                        ShowMessage(NotifyType_t.MESS_STATUS, "GS currently busy");
                        return;
                    }
                    return;
                }
            }
            m_file_open = true;
            RenderThumbs();
        }

        private void CleanUp()
        {
            /* Clear out everything */
            if (m_docPages != null && m_docPages.Count > 0)
                m_docPages.Clear();
            if (m_thumbnails != null && m_thumbnails.Count > 0)
                m_thumbnails.Clear();
            if (m_page_sizes != null && m_page_sizes.Count > 0)
                m_page_sizes.Clear();
            if (m_page_scroll_pos != null && m_page_scroll_pos.Count > 0)
                m_page_scroll_pos.Clear();

            m_GtkimageStoreThumb.Clear();
            m_GtkimageStoreMain.Clear();

            m_currpage = 0;
            m_thumbnails = new List<DocPage>();
            m_docPages = new Pages();
            m_page_sizes = new List<pagesizes_t>();
            m_page_scroll_pos = new List<int>();

            m_file_open = false;
            m_doczoom = 1.0;
            m_init_done = false;
            m_busy_render = true;
            m_firstime = true;
            m_aa = true;
            m_aa_change = false;
            m_zoom_txt = "100";
            m_page_txt = "0";
            m_GtkpageTotal.Text = "/ 0";
            m_ignore_scroll_change = false;
            m_currfile = null;
            m_origfile = null;
            m_numpages = -1;
            m_file_open = false;
            CleanUpTempFiles();
            m_tempfiles = new List<TempFile>();
            return;
        }

        private void CleanUpTempFiles()
        {
            for (int k = 0; k < m_tempfiles.Count; k++)
            {
                try
                {
                    m_tempfiles[k].DeleteFile();
                }
                catch
                {
                    ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
                }
            }
            m_tempfiles.Clear();
        }
    }
}