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
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Controls;
using GhostNET;

namespace ghostnet_wpf_example
{
	public partial class MainWindow
	{
		/* Filter so we get the first non-full res and the last
		 * full res */
		private void PageRangeRender(int first_page, int last_page)
		{
			if (m_viewer_state != ViewerState_t.DOC_OPEN)
				return;

			int real_first_page = -1;
			int real_last_page = -1;

			for (int k = first_page; k <= last_page; k++)
			{
				if (real_first_page == -1 && ((m_docPages[k].Content != Page_Content_t.FULL_RESOLUTION) ||
					(m_docPages[k].AA != m_aa)))
				{
					real_first_page = k;
				}
				if (m_docPages[k].Content != Page_Content_t.FULL_RESOLUTION || m_docPages[k].AA != m_aa)
				{
					real_last_page = k;
				}
			}

			if (real_first_page == -1)
				return;

			m_viewer_state = ViewerState_t.BUSY_RENDER;
			m_ghostscript.DisplayDeviceRunFile(m_currfile, m_doczoom, m_aa, real_first_page + 1, real_last_page + 1);
		}

		/* Callback from ghostscript with the rendered image. */
		private void MainPageCallback(int width, int height, int raster, double zoom_in,
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
			for (int k = 0; k < m_images_rendered.Count; k++)
			{
				DocPage doc_page = m_docPages[m_images_rendered[k].page_num - 1];

				if (doc_page.Content != Page_Content_t.FULL_RESOLUTION ||
					doc_page.AA != m_aa)
				{
					doc_page.Width = m_images_rendered[k].width;
					doc_page.Height = m_images_rendered[k].height;
					doc_page.Content = Page_Content_t.FULL_RESOLUTION;

					doc_page.Zoom = m_doczoom;
					doc_page.AA = m_aa;
					doc_page.BitMap = BitmapSource.Create(doc_page.Width, doc_page.Height,
						72, 72, PixelFormats.Bgr24, BitmapPalettes.Halftone256, m_images_rendered[k].bitmap, m_images_rendered[k].raster);
				}
			}

			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Collapsed;
			xaml_RenderProgress.Value = 0;
			m_images_rendered.Clear();
			m_viewer_state = ViewerState_t.DOC_OPEN;
		}

		private Tuple<int, int> GetVisiblePages()
		{
			/* First child of list view */
			Decorator border = VisualTreeHelper.GetChild(xaml_PageList, 0) as Decorator;

			// Get scrollviewer
			ScrollViewer scrollViewer = border.Child as ScrollViewer;

			double top_window = scrollViewer.VerticalOffset;
			double bottom_window = top_window + scrollViewer.ViewportHeight;
			int first_page = -1;
			int last_page = -1;

			if (top_window > m_toppage_pos[m_numpages - 1])
			{
				first_page = m_numpages - 1;
				last_page = first_page;
			}
			else
			{
				for (int k = 0; k < (m_toppage_pos.Count - 1); k++)
				{
					if (top_window <= m_toppage_pos[k + 1] && bottom_window >= m_toppage_pos[k])
					{
						if (first_page == -1)
						{
							first_page = k;
							last_page = k;
						}
						else
						{
							last_page = k;
							break;
						}
					}
					else
					{
						if (first_page != -1)
						{
							last_page = first_page;
							break;
						}
					}
				}
			}


			return new Tuple<int, int>(first_page, last_page);
		}

		/* Render only visible pages. Used for single page formats as well
		 * as PDF (and ideally XPS) where the interpreter can interpret only
		 * specified pages */
		private void RenderMainRange()
		{
			if (m_viewer_state == ViewerState_t.OPENING)
			{
				m_doczoom = 1.0;
				xaml_Zoomsize.Text = "100";
				m_ghostscript.PageRenderedCallBack += new GSNET.PageRendered(gsPageRendered);
			}

			m_viewer_state = ViewerState_t.BUSY_RENDER;
			(int first_page, int last_page) = GetVisiblePages();
			m_ghostscript.DisplayDeviceRunFile(m_currfile, m_doczoom, m_aa, first_page + 1, last_page + 1);
		}

		/* Render all pages full resolution. Used for single page files as
		 * well as documents like PostScript and PCL that are not page
		 * based indexed */
		private void RenderMainAll()
		{
			xaml_RenderProgress.Value = 0;
			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Visible;
			m_page_progress_count = 0;
			xaml_RenderProgressText.Text = "Rendering";
			if (m_viewer_state == ViewerState_t.OPENING)
			{
				m_doczoom = 1.0;
				xaml_Zoomsize.Text = "100";
				m_ghostscript.PageRenderedCallBack += new GSNET.PageRendered(gsPageRendered);
			}

			m_viewer_state = ViewerState_t.BUSY_RENDER;
			m_ghostscript.DisplayDeviceRunFile(m_currfile, m_doczoom, m_aa, -1, -1);
		}

		/* Called via zoom or aa changes */
		private void RenderMain()
		{
			if (m_viewer_state != ViewerState_t.DOC_OPEN)
				return;

			if (m_doc_type_has_page_access)
				RenderMainRange();
			else
				RenderMainAll();
		}
	}
}
