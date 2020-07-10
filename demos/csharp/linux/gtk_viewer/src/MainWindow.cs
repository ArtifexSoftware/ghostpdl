using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using Gtk;
using GhostNET;

namespace gs_mono_example
{
    static class Constants
    {
        public const double SCALE_THUMB = 0.1;
        public const int BLANK_WIDTH = 17;
        public const int BLANK_HEIGHT = 22;
        public const int DEFAULT_GS_RES = 300;
        public const int PAGE_VERT_MARGIN = 10;
        public const int MAX_PRINT_PREVIEW_LENGTH = 250;
        public const int ZOOM_MAX = 4;
        public const double ZOOM_MIN = 0.25;
    }

    public enum NotifyType_t
    {
        MESS_STATUS,
        MESS_ERROR
    };

    public enum status_t
    {
        S_ISOK,
        E_FAILURE,
        E_OUTOFMEM,
        E_NEEDPASSWORD
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
    public enum zoom_t
    {
        NO_ZOOM,
        ZOOM_IN,
        ZOOM_OUT
    }

    public enum doc_t
    {
        UNKNOWN,
        PDF,
        PS,
        PCL,
        XPS
    }

    public struct idata_t
    {
        public int page_num;
        public Byte[] bitmap;
        public int height;
        public int width;
        public int raster;
        public double zoom;
    }

    public struct pagesizes_t
    {
        public double width;
        public double height;
        public double cummulative_y;
    }

    public partial class MainWindow : Gtk.Window
    {
        ghostsharp m_ghostscript;
        bool m_file_open;
        doc_t m_document_type;
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
        List<idata_t> m_images_rendered;
        bool m_init_done;
        bool m_busy_render;
        bool m_firstime;
        bool m_validZoom;
        bool m_aa;
        bool m_aa_change;
        List<int> m_toppage_pos;
        int m_page_progress_count;
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

        void ShowMessage(NotifyType_t type, string message)
        {
            MessageDialog md = new MessageDialog(this,
            DialogFlags.DestroyWithParent, MessageType.Error,
            ButtonsType.Close, message);

            if (type == NotifyType_t.MESS_ERROR)
                md.Title = "Error";
            else
                md.Title = "Notice";
            md.ShowAll();
        }

        public MainWindow() : base(Gtk.WindowType.Toplevel)
        {
            /* Set up ghostscript calls for progress update */
            m_ghostscript = new ghostsharp();
            m_ghostscript.gsUpdateMain += new ghostsharp.gsCallBackMain(gsProgress);
            m_ghostscript.gsIOUpdateMain += new ghostsharp.gsIOCallBackMain(gsIO);
            m_ghostscript.gsDLLProblemMain += new ghostsharp.gsDLLProblem(gsDLL);

            DeleteEvent += delegate { Application.Quit(); };

            m_currpage = 0;
            m_gsoutput = new gsOutput();
            m_gsoutput.Activate();
            m_tempfiles = new List<TempFile>();
            m_thumbnails = new List<DocPage>();
            m_docPages = new Pages();
            m_page_sizes = new List<pagesizes_t>();
            m_file_open = false;
            m_document_type = doc_t.UNKNOWN;
            m_doczoom = 1.0;
            m_init_done = false;
            m_busy_render = true;
            m_validZoom = true;
            m_firstime = true;
            m_images_rendered = new List<idata_t>();
            m_aa = true;
            m_aa_change = false;
            Gtk.TextTagTable tag = new Gtk.TextTagTable(IntPtr.Zero);
           
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

            SeparatorMenuItem sep = new SeparatorMenuItem();
            filemenu.Append(sep);

            ImageMenuItem quiti = new ImageMenuItem(Stock.Quit, agr);
            quiti.AddAccelerator("activate", agr, new AccelKey(
                Gdk.Key.q, Gdk.ModifierType.ControlMask, AccelFlags.Visible));
            filemenu.Append(quiti);
            quiti.Activated += OnQuit;

            menu_bar.Append(file);

            Menu aboutmenu = new Menu();
            MenuItem about = new MenuItem("About");
            about.Submenu = aboutmenu;
            menu_bar.Append(about);

            m_GtkvBoxMain.PackStart(menu_bar, false, false, 0);

            /* Add a hbox with the page information, zoom control, and aa to vbox */
            HBox pageBox = new HBox(false, 0);
            m_GtkpageEntry = new Entry();
            m_GtkpageEntry.WidthChars = 4;
            m_GtkpageTotal = new Label("/0");
            pageBox.PackStart(m_GtkpageEntry, false, false, 0);
            pageBox.PackStart(m_GtkpageTotal, false, false, 0);

            HBox zoomBox = new HBox(false, 0);
            Button zoomPlus = new Button();
            zoomPlus.Label = "+";
            Button zoomMinus = new Button();
            zoomMinus.Label = "–";
            m_GtkzoomEntry = new Entry();
            m_GtkzoomEntry.WidthChars = 3;
            Label precentLabel = new Label("%");
            zoomBox.PackStart(zoomPlus, false, false, 0);
            zoomBox.PackStart(zoomMinus, false, false, 0);
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

            m_GtkTreeThumb = new Gtk.TreeView();
            m_GtkTreeThumb.HeadersVisible = false;
            m_GtkimageStoreThumb = new Gtk.ListStore(typeof(Gdk.Pixbuf));
            m_GtkTreeThumb.AppendColumn("Thumb", new Gtk.CellRendererPixbuf(), "pixbuf", 0);
            m_GtkTreeThumb.Style.YThickness = 100;
            m_GtkthumbScroll.Add(m_GtkTreeThumb);

           /* var colmn = m_GtkTreeThumb.Columns;
            var mycol = (Gtk.TreeViewColumn)colmn.GetValue(0);
            mycol.Spacing = 40;
            mycol.FixedWidth = 0;*/

            m_GtkTreeMain = new Gtk.TreeView();
            m_GtkTreeMain.HeadersVisible = false;
            m_GtkimageStoreMain = new Gtk.ListStore(typeof(Gdk.Pixbuf));
            m_GtkTreeMain.AppendColumn("Main", new Gtk.CellRendererPixbuf(), "pixbuf", 0);
            m_GtkmainScroll.Add(m_GtkTreeMain);

            hBoxPages.PackStart(m_GtkthumbScroll, false, false, 0);
            hBoxPages.PackStart(m_GtkmainScroll, true, true, 0);

            m_GtkTreeThumb.Model = m_GtkimageStoreThumb;
            m_GtkTreeMain.Model = m_GtkimageStoreMain;



            m_GtkvBoxMain.PackStart(hBoxPages, true, true, 0);

            /* Progress bar */
            m_GtkProgressBox = new HBox(false, 0);
            m_GtkProgressBar = new ProgressBar();
            m_GtkProgressBar.Orientation = ProgressBarOrientation.LeftToRight;
            m_GtkProgressBox.PackStart(m_GtkProgressBar, true, true, 0);
            m_GtkProgressLabel = new Label("Render Thumbnails");
            m_GtkProgressBox.PackStart(m_GtkProgressLabel, false, false, 0);

            m_GtkvBoxMain.PackStart(m_GtkProgressBox, false, false, 0);
            //m_GtkvBoxMain.Remove(m_GtkProgressBox);
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
            throw new NotImplementedException();
        }

        private void gsIO(String mess, int len)
        {
            Gtk.TextBuffer buffer = m_gsoutput.m_textView.Buffer;
            Gtk.TextIter ti = buffer.EndIter;

            try
            {
                var part = mess.Substring(0, len);
                buffer.Insert(ref ti, part);
            }
            catch(Exception except)
            {
                var issue = except.Message;
            }
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

        private void gsProgress(gsEventArgs asyncInformation)
        {
            if (asyncInformation.Completed)
            {
                switch (asyncInformation.Params.task)
                {

                    case GS_Task_t.PS_DISTILL:
                        //xaml_DistillProgress.Value = 100;
                        //xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
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
                if (asyncInformation.Params.result == GS_Result_t.gsFAILED)
                {
                    switch (asyncInformation.Params.task)
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
                GSResult(asyncInformation.Params);
            }
            else
            {
                switch (asyncInformation.Params.task)
                {
                    case GS_Task_t.CREATE_XPS:
                        // this.xaml_DistillProgress.Value = asyncInformation.Progress;
                        break;

                    case GS_Task_t.PS_DISTILL:
                        // this.xaml_DistillProgress.Value = asyncInformation.Progress;
                        break;

                    case GS_Task_t.SAVE_RESULT:
                        break;
                }
            }
        }

        /* GS Result*/
        public void GSResult(gsParamState_t gs_result)
        {
#if false
            // TempFile tempfile = null;

            // if (gs_result.outputfile != null)
            //     tempfile = new TempFile(gs_result.outputfile);

            if (gs_result.result == GS_Result_t.gsCANCELLED)
            {
               // xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
                if (tempfile != null)
                {
                    try
                    {
                        tempfile.DeleteFile();
                    }
                    catch
                    {
                        ShowMessage(this, NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
                    }
                }
                return;
            }
            if (gs_result.result == GS_Result_t.gsFAILED)
            {
               //xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
                ShowMessage(this, NotifyType_t.MESS_STATUS, "GS Failed Conversion");
                if (tempfile != null)
                {
                    try
                    {
                        tempfile.DeleteFile();
                    }
                    catch
                    {
                        ShowMessage(this, NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
                    }
                }
                return;
            }
#endif
            switch (gs_result.task)
            {
#if false
                case GS_Task_t.PS_DISTILL:
                    //xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
                    m_origfile = gs_result.inputfile;

                    /* Save distilled result */
                    SaveFileDialog dlg = new SaveFileDialog();
                    dlg.Filter = "PDF file (*.pdf)|*.pdf";
                    dlg.FileName = System.IO.Path.GetFileNameWithoutExtension(m_origfile) + ".pdf";
                    if (dlg.ShowDialog() == true)
                    {
                        try
                        {
                            if (File.Exists(dlg.FileName))
                            {
                                File.Delete(dlg.FileName);
                            }
                            File.Copy(tempfile.Filename, dlg.FileName);
                        }
                        catch (Exception except)
                        {
                            ShowMessage(this, NotifyType_t.MESS_ERROR, "Exception Saving Distilled Result:" + except.Message);
                        }

                    }
                    tempfile.DeleteFile();
                    break;
#endif

                case GS_Task_t.SAVE_RESULT:
                    /* Don't delete file in this case as this was our output! */
                    ShowMessage(NotifyType_t.MESS_STATUS, "GS Completed Conversion");
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
            if (response == (int)Gtk.ResponseType.Accept)
            {
                m_currfile = dialog.Filename;
                m_extension = m_currfile.Split('.')[1];
            }
            dialog.Destroy();

            RenderThumbs();
        }
    }
}
