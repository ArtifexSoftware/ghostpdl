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
			doc_page.BitMap = m_thumbnails[page_num - 1].BitMap;
			doc_page.Width = (int)(width / (Constants.SCALE_THUMB));
			doc_page.Height = (int)(height / (Constants.SCALE_THUMB));
			doc_page.PageNum = page_num;
			m_docPages.Add(doc_page);
			//m_toppage_pos.Add(offset + Constants.PAGE_VERT_MARGIN);
			//offset += doc_page.Height + Constants.PAGE_VERT_MARGIN;
		}

		/* Rendered all the thumbnail pages.  Stick them in the appropriate lists */
		private void ThumbsDone()
		{
           	//m_toppage_pos.Add(offset);
			//xaml_ProgressGrid.Visibility = System.Windows.Visibility.Collapsed;
			//xaml_RenderProgress.Value = 0;
			//xaml_PageList.ItemsSource = m_docPages;
			//xaml_ThumbList.ItemsSource = m_thumbnails;
			//xaml_ThumbList.Items.Refresh();
			
			//m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);

			m_numpages = m_thumbnails.Count;
			if (m_numpages < 1)
			{
				//ShowMessage(NotifyType_t.MESS_STATUS, "File failed to open properly");
				//CleanUp();
			}
			else
			{
				m_init_done = true;
				//xaml_TotalPages.Text = "/" + m_numpages;
				//xaml_currPage.Text = m_currpage.ToString();

				/* If non-pdf, kick off full page rendering */
				//RenderMainFirst();
			}
		}

		/* Callback from ghostscript with the rendered thumbnail.  Also update progress */
		private void ThumbPageCallback(object gsObject, int width, int height, int raster, double zoom_in,
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

            GCHandle pinned = GCHandle.Alloc(bitmap, GCHandleType.Pinned);
            IntPtr address = pinned.AddrOfPinnedObject();
            doc_page.BitMap = new Bitmap(doc_page.Width, doc_page.Height, raster,
                System.Drawing.Imaging.PixelFormat.Format24bppRgb, address);
            pinned.Free();

            ThumbAssignMain(page_num, width, height, 1.0, ref offset);

            /* Dispatch progress bar update on UI thread */
#if false
            System.Windows.Application.Current.Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Send, new Action(() =>
			{
				/* Logrithmic but it will show progress */
			//xaml_RenderProgress.Value = ((double) page_num / ((double) page_num + 1))* 100.0;
			}));
#endif
		}

		/* Render the thumbnail images */
		private void RenderThumbs()
		{
            m_GtkvBoxMain.PackStart(m_GtkProgressBox, false, false, 0);
            m_GtkProgressLabel.Text = "Rendering Thumbs";
            m_GtkProgressBar.Fraction = 0.0;

            m_ghostscript.gsPageRenderedMain += new ghostsharp.gsCallBackPageRenderedMain(gsThumbRendered);
            m_ghostscript.gsDisplayDeviceRenderAll(m_currfile, Constants.SCALE_THUMB, false, GS_Task_t.DISPLAY_DEV_THUMBS_NON_PDF);
		}
	}
}