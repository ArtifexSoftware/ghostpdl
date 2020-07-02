using System;
using System.Windows.Media;
using System.Collections.Generic;
using System.Windows.Media.Imaging;
using System.Runtime.InteropServices;
using GhostNET;

namespace ghostnet_wpf_example
{
	public partial class MainWindow
	{
		bool m_busy_rendering;
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

			m_busy_rendering = true;
			m_firstpage = first_page;
			m_lastpage = last_page;
			//m_ghostscript.gsDisplayDeviceRender(m_currfile, first_page + 1, last_page + 1, 1.0);
		}

		/* Callback from ghostscript with the rendered image. */
		private void MainPageCallback(object gsObject, int width, int height, int raster, double zoom_in,
			int page_num, IntPtr data)
		{
			Byte[] bitmap = new byte[raster * height];
			idata_t image_data = new idata_t();

			Marshal.Copy(data, bitmap, 0, raster * height);

			image_data.bitmap = bitmap;
			image_data.page_num = page_num;
			image_data.width = width;
			image_data.height = height;
			image_data.raster = raster;
			image_data.zoom = zoom_in;
			m_images_rendered.Add(image_data);

			/* Get the 1.0 page scalings */
			if (m_firstime)
			{
				pagesizes_t page_size = new pagesizes_t();
				page_size.size.X = width;
				page_size.size.Y = height;
				m_page_sizes.Add(page_size);
			}

			/* Dispatch progress bar update on UI thread */
			System.Windows.Application.Current.Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Send, new Action(() =>
			{
				m_page_progress_count += 1;
				xaml_RenderProgress.Value = ((double)m_page_progress_count / (double) m_numpages) * 100.0;
			}));
		}

		/* Done rendering. Update the pages with the new raster information if needed */
		private void RenderingDone()
		{
			int page_index = m_firstpage - 1;
			m_toppage_pos = new List<int>(m_images_rendered.Count + 1);
			int offset = 0;

			for (int k = 0; k < m_images_rendered.Count; k++)
			{
				DocPage doc_page = m_docPages[page_index + k];

				if (doc_page.Content != Page_Content_t.FULL_RESOLUTION ||
					m_aa_change)
				{
					doc_page.Width = m_images_rendered[k].width;
					doc_page.Height = m_images_rendered[k].height;
					doc_page.Content = Page_Content_t.FULL_RESOLUTION;

					doc_page.Zoom = m_doczoom;
					doc_page.BitMap = BitmapSource.Create(doc_page.Width, doc_page.Height,
						72, 72, PixelFormats.Bgr24, BitmapPalettes.Halftone256, m_images_rendered[k].bitmap, m_images_rendered[k].raster);
				}
				m_toppage_pos.Add(offset + Constants.PAGE_VERT_MARGIN);
				offset += doc_page.Height + Constants.PAGE_VERT_MARGIN;
			}

			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Collapsed;
			xaml_RenderProgress.Value = 0;
			m_aa_change = false;
			m_firstime = false;
			m_toppage_pos.Add(offset);
			m_busy_rendering = false;
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
			xaml_RenderProgress.Value = 0;
			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Visible;
			m_page_progress_count = 0;
			xaml_RenderProgressText.Text = "Rendering";
			if (m_firstime)
			{
				xaml_Zoomsize.Text = "100";
			}

			m_ghostscript.gsPageRenderedMain += new ghostsharp.gsCallBackPageRenderedMain(gsPageRendered);
			m_ghostscript.gsDisplayDeviceRenderAll(m_currfile, m_doczoom, m_aa, GS_Task_t.DISPLAY_DEV_NON_PDF);
		}

		/* Render all, but only if not already busy,  called via zoom or aa changes */
		private void RenderMainAll()
		{
			if (m_busy_render || !m_init_done)
				return;
			RenderMainFirst();
		}
	}
}
