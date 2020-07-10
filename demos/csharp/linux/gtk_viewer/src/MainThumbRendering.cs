using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using GhostNET;

namespace gs_mono_example
{
	public partial class MainWindow
	{
	    private static List<DocPage> m_thumbnails;

		/* Assign current pages to blown up thumbnail images */
		private void ThumbAssignMain(int page_num, int width, int height, double zoom_in, ref int offset)
		{
			DocPage doc_page = new DocPage();
			doc_page.Content = Page_Content_t.THUMBNAIL;
			doc_page.Zoom = zoom_in;
            doc_page.Width = (int)(width / (Constants.SCALE_THUMB));
            doc_page.Height = (int)(height / (Constants.SCALE_THUMB));
            doc_page.PixBuf = m_thumbnails[page_num - 1].PixBuf.ScaleSimple(doc_page.Width, doc_page.Height, Gdk.InterpType.Nearest);
			doc_page.PageNum = page_num;
			m_docPages.Add(doc_page);
			//m_toppage_pos.Add(offset + Constants.PAGE_VERT_MARGIN);
			//offset += doc_page.Height + Constants.PAGE_VERT_MARGIN;
		}

		/* Rendered all the thumbnail pages.  Stick them in the appropriate lists */
		private void ThumbsDone()
		{
            //m_GtkvBoxMain.Remove(m_GtkProgressBox);
            m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);
            m_numpages = m_thumbnails.Count;
			if (m_numpages < 1)
			{
				ShowMessage(NotifyType_t.MESS_STATUS, "File failed to open properly");
				//CleanUp();
			}
			else
			{
				m_init_done = true;
                m_GtkpageEntry.Text = "1";
                m_GtkpageTotal.Text = "/" + m_numpages;
                m_GtkzoomEntry.Text = "100";
                for (int k = 0; k < m_numpages; k++)
                {
                    m_GtkimageStoreThumb.AppendValues(m_thumbnails[k].PixBuf);
                    m_GtkimageStoreMain.AppendValues(m_docPages[k].PixBuf);
                }

              //  var colmn = m_GtkTreeThumb.Columns;
              //  var mycol = (Gtk.TreeViewColumn)colmn.GetValue(0);

                //mycol.IsFloating = true;


                /* If non-pdf, kick off full page rendering */
                RenderMainFirst();
            }
        }

        /* Callback from ghostscript with the rendered thumbnail.  Also update progress */
        private void ThumbPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            Byte[] bitmap = new byte[raster * height];
            int offset = 0;

            Marshal.Copy(data, bitmap, 0, raster * height);
            DocPage doc_page = new DocPage();
            m_thumbnails.Add(doc_page);

            doc_page.Width = width;
            doc_page.Height = height;
            doc_page.Content = Page_Content_t.THUMBNAIL;
            doc_page.Zoom = zoom_in;
            doc_page.PageNum = page_num;

            doc_page.PixBuf = new Gdk.Pixbuf(bitmap, Gdk.Colorspace.Rgb, false, 8, width, height, raster);
            ThumbAssignMain(page_num, width, height, 1.0, ref offset);

            /* Dispatch progress bar update on UI thread */
            Gtk.Application.Invoke(delegate {
                m_GtkProgressBar.Fraction = ((double)page_num / ((double)page_num + 1));
            });
		}

		/* Render the thumbnail images */
		private void RenderThumbs()
		{
           //m_GtkvBoxMain.PackStart(m_GtkProgressBox, false, false, 0);
            m_GtkProgressLabel.Text = "Rendering Thumbs";
            m_GtkProgressBar.Fraction = 0.0;

            m_ghostscript.gsPageRenderedMain += new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);
            m_ghostscript.gsDisplayDeviceRenderAll(m_currfile, Constants.SCALE_THUMB, false, GS_Task_t.DISPLAY_DEV_THUMBS_NON_PDF);
		}
	}
}