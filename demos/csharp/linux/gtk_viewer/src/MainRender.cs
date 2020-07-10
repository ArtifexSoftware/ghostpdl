using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using GhostNET;

namespace gs_mono_example
{
	public partial class MainWindow
	{
		int m_firstpage;
		int m_lastpage;

		/* For PDF optimization */
		private void PageRangeRender(int first_page, int last_page)
		{
			bool render_pages = false;
			for (int k = first_page; k <= last_page; k++)
			{
				if (m_docPages[k].Content != Page_Content_t.FULL_RESOLUTION)
				{
					render_pages = true;
					break;
				}
			}
			if (!render_pages)
				return;

			m_busy_render = true;
			m_firstpage = first_page;
			m_lastpage = last_page;
			//m_ghostscript.gsDisplayDeviceRender(m_currfile, first_page + 1, last_page + 1, 1.0);
		}

        /* Callback from ghostscript with the rendered image. */
        private void MainPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            try
            {
                Byte[] bitmap = new byte[raster * height];

                Marshal.Copy(data, bitmap, 0, raster * height);
                DocPage doc_page = m_docPages[page_num - 1];

                if (doc_page.Content != Page_Content_t.FULL_RESOLUTION ||
                    m_aa_change)
                {
                    doc_page.Width = width;
                    doc_page.Height = height;
                    doc_page.Content = Page_Content_t.FULL_RESOLUTION;
                    doc_page.Zoom = m_doczoom;
                    doc_page.PixBuf = new Gdk.Pixbuf(bitmap, Gdk.Colorspace.Rgb, false, 8, width, height, raster);
                }

                /* Get the 1.0 page scalings */
                if (m_firstime)
                {
                    pagesizes_t page_size = new pagesizes_t();
                    page_size.width = width;
                    page_size.height = height;
                    m_page_sizes.Add(page_size);
                }
            }
            catch (Exception except)
            {
                var t = except.Message;
            }

            /* Dispatch progress bar update on UI thread */
            Gtk.Application.Invoke(delegate {
                m_GtkProgressBar.Fraction = ((double)page_num / ((double)m_numpages));
            });
        }

		/* Done rendering. Update the pages with the new raster information if needed */
		private void RenderingDone()
		{
			int page_index = m_firstpage - 1;
			m_toppage_pos = new List<int>(m_images_rendered.Count + 1);
			int offset = 0;
            Gtk.TreeIter tree_iter;

            m_GtkimageStoreMain.GetIterFirst(out tree_iter);
            for (int k = 0; k < m_numpages; k++)
            {
                m_GtkimageStoreMain.SetValue(tree_iter, 0, m_docPages[k].PixBuf);
                m_GtkimageStoreMain.IterNext(ref tree_iter);
            }

            m_GtkaaCheck.Sensitive = true;
            m_aa_change = false;
			m_firstime = false;
			m_toppage_pos.Add(offset);
			m_busy_render = false;
			m_images_rendered.Clear();
			m_file_open = true;
			m_busy_render = false;

			m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsPageRendered);
		}

		/* Render all pages full resolution */
		private void RenderMainFirst()
		{
			m_firstpage = 1;
			m_busy_render = true;
			m_page_progress_count = 0;
            m_ghostscript.gsPageRenderedMain += new ghostsharp.gsCallBackPageRenderedMain(gsPageRendered);
			m_ghostscript.gsDisplayDeviceRenderAll(m_currfile, m_doczoom, m_aa, GS_Task_t.DISPLAY_DEV_NON_PDF);
		}

		/* Render all, but only if not already busy,  called via zoom or aa changes */
		private void RenderMainAll()
		{
			if (m_busy_render || !m_init_done)
				return;
            m_GtkaaCheck.Sensitive = false;
            RenderMainFirst();
		}
	}
}
