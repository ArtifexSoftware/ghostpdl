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
		private void ThumbAssignMain(int page_num, int width, int height, double zoom_in)
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

        /* In MS NET version this is returned on the main UI thread.  Not
           so in MONO .NET  */
        private void ThumbsDone()
        {
            /* Dispatch on UI thread */
            Gtk.Application.Invoke(delegate {
                ThumbsDoneMain();
            });
        }

        private void ThumbsDoneMain()
        { 
            //m_GtkvBoxMain.Remove(m_GtkProgressBox);
            m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);
            m_numpages = m_thumbs_rendered.Count;
            if (m_numpages < 1)
            {
                ShowMessage(NotifyType_t.MESS_STATUS, "File failed to open properly");
                //CleanUp();
            }
            else
            {
                for (int k = 0; k < m_thumbs_rendered.Count; k++)
                {
                    DocPage doc_page = new DocPage();
                    m_thumbnails.Add(doc_page);

                    doc_page.Width = m_thumbs_rendered[k].width;
                    doc_page.Height = m_thumbs_rendered[k].height;
                    doc_page.Content = Page_Content_t.THUMBNAIL;
                    doc_page.Zoom = m_thumbs_rendered[k].zoom;

                    doc_page.PixBuf = new Gdk.Pixbuf(m_thumbs_rendered[k].bitmap,
                        Gdk.Colorspace.Rgb, false, 8, m_thumbs_rendered[k].width,
                        m_thumbs_rendered[k].height, m_thumbs_rendered[k].raster);
                    doc_page.PageNum = m_thumbs_rendered[k].page_num;

                    ThumbAssignMain(m_thumbs_rendered[k].page_num,
                        m_thumbs_rendered[k].width, m_thumbs_rendered[k].height, 
                        1.0);
                }

                m_init_done = true;
                m_GtkpageEntry.Text = "1";
                m_GtkpageTotal.Text = "/" + m_numpages;
                m_GtkzoomEntry.Text = "100";
                for (int k = 0; k < m_numpages; k++)
                {
                    m_GtkimageStoreThumb.AppendValues(m_thumbnails[k].PixBuf);
                    m_GtkimageStoreMain.AppendValues(m_docPages[k].PixBuf);
                }

                m_thumbs_rendered.Clear();

                /* If non-pdf, kick off full page rendering */
                RenderMainFirst();
            }

            //m_GtkvBoxMain.Remove(m_GtkProgressBox);
            m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);
            m_numpages = m_thumbnails.Count;

        }

        /* Callback from ghostscript with the rendered thumbnail.  Also update progress */
        private void ThumbPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            Byte[] bitmap = new byte[raster * height];

            idata_t thumb = new idata_t();

            Marshal.Copy(data, bitmap, 0, raster * height);

            thumb.bitmap = bitmap;
            thumb.page_num = page_num;
            thumb.width = width;
            thumb.height = height;
            thumb.raster = raster;
            thumb.zoom = zoom_in;
            m_thumbs_rendered.Add(thumb);

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