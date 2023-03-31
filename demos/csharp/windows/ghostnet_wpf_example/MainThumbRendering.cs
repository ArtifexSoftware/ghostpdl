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
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Runtime.InteropServices;
using GhostNET;

namespace ghostnet_wpf_example
{
	public partial class MainWindow
	{
		private static List<DocPage> m_thumbnails;

		/* Assign current pages to blown up thumbnail images */
		private void ThumbAssignMain(int page_num, int width, int height, double zoom_in, ref int offset)
		{
			m_pageType.Add(Page_Content_t.THUMBNAIL);
			DocPage doc_page = new DocPage();
			doc_page.Content = Page_Content_t.THUMBNAIL;
			doc_page.Zoom = zoom_in;
			doc_page.AA = m_aa;
			doc_page.BitMap = m_thumbnails[page_num - 1].BitMap;
			doc_page.Width = (int)(width / (Constants.SCALE_THUMB));
			doc_page.Height = (int)(height / (Constants.SCALE_THUMB));
			doc_page.PageNum = page_num;
			m_docPages.Add(doc_page);

			/* Set the page offsets.  Used for determining which pages
			 * will be visible within viewport. */
			m_toppage_pos.Add(offset + Constants.PAGE_VERT_MARGIN);
			offset += doc_page.Height + 2 * Constants.PAGE_VERT_MARGIN;

			/* Set page sizes for 1.0 scaling. This is used to quick
			 * rescale of pages prior to rendering at new zoom. */
			pagesizes_t page_size = new pagesizes_t();
			page_size.size.X = doc_page.Width;
			page_size.size.Y = doc_page.Height;
			m_page_sizes.Add(page_size);
		}

		/* Rendered all the thumbnail pages.  Stick them in the appropriate lists */
		private void ThumbsDone()
		{
			int offset = 0;
			m_toppage_pos = new List<int>(m_list_thumb.Count);

			for (int k = 0; k < m_list_thumb.Count; k++)
			{
				DocPage doc_page = new DocPage();
				m_thumbnails.Add(doc_page);

				doc_page.Width = m_list_thumb[k].width;
				doc_page.Height = m_list_thumb[k].height;
				doc_page.Content = Page_Content_t.THUMBNAIL;
				doc_page.Zoom = m_list_thumb[k].zoom;
				doc_page.BitMap = BitmapSource.Create(doc_page.Width, doc_page.Height,
					72, 72, PixelFormats.Bgr24, BitmapPalettes.Halftone256, m_list_thumb[k].bitmap, m_list_thumb[k].raster);
				doc_page.PageNum = m_list_thumb[k].page_num;
				ThumbAssignMain(m_list_thumb[k].page_num, m_list_thumb[k].width, m_list_thumb[k].height, 1.0, ref offset);
			}

			m_toppage_pos.Add(offset);
			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Collapsed;
			xaml_RenderProgress.Value = 0;
			xaml_PageList.ItemsSource = m_docPages;
			xaml_ThumbList.ItemsSource = m_thumbnails;
			xaml_ThumbList.Items.Refresh();
			xaml_ThumbGrid.Visibility = System.Windows.Visibility.Visible;

			m_ghostscript.PageRenderedCallBack -= new GSNET.PageRendered(gsThumbRendered);

			m_numpages = m_list_thumb.Count;
			if (m_numpages < 1)
			{
				ShowMessage(NotifyType_t.MESS_STATUS, "File failed to open properly");
				CleanUp();
			}
			else
			{
				xaml_TotalPages.Text = "/" + m_numpages;
				xaml_currPage.Text = m_currpage.ToString();
				m_list_thumb.Clear();

				if (m_doc_type_has_page_access)
					RenderMainRange();
				else
					RenderMainAll();
			}
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
			m_list_thumb.Add(thumb);

			/* Dispatch progress bar update on UI thread */
			System.Windows.Application.Current.Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Send, new Action(() =>
			{
				/* Logrithmic but it will show progress */
				xaml_RenderProgress.Value = ((double) page_num / ((double) page_num + 1))* 100.0;
			}));
		}

		/* Render the thumbnail images */
		private void RenderThumbs()
		{
			xaml_RenderProgress.Value = 0;
			xaml_RenderProgressText.Text = "Creating Thumbs";
			xaml_ProgressGrid.Visibility = System.Windows.Visibility.Visible;

			m_ghostscript.PageRenderedCallBack += new GSNET.PageRendered(gsThumbRendered);
			m_ghostscript.DisplayDeviceRenderThumbs(m_currfile, Constants.SCALE_THUMB, false);
		}
	}
}
