using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using Gtk;
using gtk_viewer.src;
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
        List<idata_t> m_list_thumb;
        List<idata_t> m_images_rendered;
        bool m_init_done;
        bool m_busy_render;
        bool m_firstime;
        bool m_validZoom;
        bool m_aa;
        bool m_aa_change;
        List<int> m_toppage_pos;
        int m_page_progress_count;


        void ShowMessage(Window parent, NotifyType_t type, string message)
        {
            Dialog dialog = null;
            String title = "Notice";

            if (type == NotifyType_t.MESS_ERROR)
                title = "Error";

            try
            {
                dialog = new Dialog(title, parent,
                    DialogFlags.DestroyWithParent | DialogFlags.Modal,
                    ResponseType.Ok);
                dialog.VBox.Add(new Label(message));
                dialog.ShowAll();

                dialog.Run();
            }
            finally
            {
                if (dialog != null)
                    dialog.Destroy();
            }
        }

        public MainWindow() : base(Gtk.WindowType.Toplevel)
        {
            /* Set up ghostscript calls for progress update */
            m_ghostscript = new ghostsharp();
            m_ghostscript.gsUpdateMain += new ghostsharp.gsCallBackMain(gsProgress);
            m_ghostscript.gsIOUpdateMain += new ghostsharp.gsIOCallBackMain(gsIO);
            m_ghostscript.gsDLLProblemMain += new ghostsharp.gsDLLProblem(gsDLL);

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
            m_list_thumb = new List<idata_t>();
            m_images_rendered = new List<idata_t>();
            m_busy_rendering = false;
            m_aa = true;
            m_aa_change = false;
        }

        private void gsIO(object gsObject, String mess, int len)
        {
            //m_gsoutput.Update(mess, len);
        }

        private void gsDLL(object gsObject, String mess)
        {
            ShowMessage(this, NotifyType_t.MESS_STATUS, mess);
        }

        private void gsProgress(object gsObject, gsEventArgs asyncInformation)
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
                        //ThumbsDone();
                        break;

                    case GS_Task_t.DISPLAY_DEV_PDF:
                    case GS_Task_t.DISPLAY_DEV_NON_PDF:
                        //RenderingDone();
                        break;

                }
                if (asyncInformation.Params.result == GS_Result_t.gsFAILED)
                {
                    switch (asyncInformation.Params.task)
                    {
                        case GS_Task_t.CREATE_XPS:
                            ShowMessage(this, NotifyType_t.MESS_STATUS, "Ghostscript failed to create XPS");
                            break;

                        case GS_Task_t.PS_DISTILL:
                            ShowMessage(this, NotifyType_t.MESS_STATUS, "Ghostscript failed to distill PS");
                            break;

                        case GS_Task_t.SAVE_RESULT:
                            ShowMessage(this, NotifyType_t.MESS_STATUS, "Ghostscript failed to convert document");
                            break;

                        default:
                            ShowMessage(this, NotifyType_t.MESS_STATUS, "Ghostscript failed.");
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
                    ShowMessage(this, NotifyType_t.MESS_STATUS, "GS Completed Conversion");
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
