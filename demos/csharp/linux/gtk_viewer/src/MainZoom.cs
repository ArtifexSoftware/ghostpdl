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
using System.Text.RegularExpressions;

namespace gs_mono_example
{
	public partial class MainWindow
	{
		static double[] ZoomSteps = new double[]
		{0.25, 0.333, 0.482, 0.667, 0.75, 1.0, 1.25, 1.37,
			1.50, 2.00, 3.00, 4.00};

		public double GetNextZoom(double curr_zoom, int direction)
		{
			int k = 0;

			/* Find segement we are in */
			for (k = 0; k < ZoomSteps.Length - 1; k++)
			{
				if (curr_zoom >= ZoomSteps[k] && curr_zoom <= ZoomSteps[k + 1])
					break;
			}

			/* Handling increasing zoom case.  Look at upper boundary */
			if (curr_zoom < ZoomSteps[k + 1] && direction > 0)
				return ZoomSteps[k + 1];

			if (Math.Abs(curr_zoom - ZoomSteps[k + 1]) <= Single.Epsilon && direction > 0)
			{
				if (k + 1 < ZoomSteps.Length - 1)
					return ZoomSteps[k + 2];
				else
					return ZoomSteps[k + 1];
			}

			/* Handling decreasing zoom case.  Look at lower boundary */
			if (curr_zoom > ZoomSteps[k] && direction < 0)
				return ZoomSteps[k];

			if (Math.Abs(curr_zoom - ZoomSteps[k]) <= Single.Epsilon && direction < 0)
			{
				if (k > 0)
					return ZoomSteps[k - 1];
				else
					return ZoomSteps[k];
			}
			return curr_zoom;
		}

		private bool ZoomDisabled()
		{
			if (!m_init_done || m_busy_render)
				return true;
			else
				return false;
		}

		private void ZoomOut(object sender, EventArgs e)
        {
			if (ZoomDisabled())
				return;
			if (!m_init_done || m_doczoom <=  Constants.ZOOM_MIN)
				return;

            m_busy_render = true;
            m_doczoom = GetNextZoom(m_doczoom, -1);
            m_GtkzoomEntry.Text = Math.Round(m_doczoom * 100.0).ToString();
            m_zoom_txt = m_GtkzoomEntry.Text;
            ResizePages();
			RenderMainAll();
		}

		private void ZoomIn(object sender, EventArgs e)
        {
			if (ZoomDisabled())
				return;
			if (!m_init_done || m_doczoom >= Constants.ZOOM_MAX)
				return;

            m_busy_render = true;
            m_doczoom = GetNextZoom(m_doczoom, 1);
		    m_GtkzoomEntry.Text = Math.Round(m_doczoom * 100.0).ToString();
            m_zoom_txt = m_GtkzoomEntry.Text;
            ResizePages();
			RenderMainAll();
		}

        void ZoomChanged(object sender, EventArgs e)
        {
            Regex regex = new Regex("[^0-9.]+");

            var text_entered = m_GtkzoomEntry.Text;
            if (text_entered == "")
            {
                return;
            }

            if (!m_init_done)
            {
                m_GtkzoomEntry.Text = "100";
                return;
            }

            bool ok = !regex.IsMatch(text_entered);
            if (!ok)
            {
                m_GtkzoomEntry.Text = m_zoom_txt;
                return;
            }

            double zoom = (double)System.Convert.ToDouble(text_entered);
            zoom = zoom / 100.0;

            if (zoom > Constants.ZOOM_MAX)
            {
                zoom = Constants.ZOOM_MAX;
                m_GtkzoomEntry.Text = (zoom * 100).ToString();
            }
            if (zoom < Constants.ZOOM_MIN)
            {
                zoom = Constants.ZOOM_MIN;
                m_GtkzoomEntry.Text = (zoom * 100).ToString();
            }

            m_doczoom = zoom;
            m_busy_render = true;
            m_zoom_txt = m_GtkzoomEntry.Text;
            ResizePages();
            RenderMainAll();
        }

        void PageChanged(object sender, EventArgs e)
        {
            Regex regex = new Regex("[^0-9.]+");

            var text_entered = m_GtkpageEntry.Text;
            if (text_entered == "")
            {
                return;
            }

            if (!m_init_done)
            {
                m_GtkpageEntry.Text = "";
                return;
            }

            bool ok = !regex.IsMatch(text_entered);
            if (!ok)
            {
                m_GtkpageEntry.Text = m_page_txt;
                return;
            }

            int page = (Int32)System.Convert.ToInt32(text_entered);
            if (page > m_numpages)
            {
                page = m_numpages;
            }
            if (page < 1)
            {
                page = 1;
            }

            m_GtkpageEntry.Text = page.ToString();
            m_page_txt = m_GtkpageEntry.Text;
            ScrollMainTo(page - 1);
        }

        private void ResizePages()
		{
            Gtk.TreeIter tree_iter;
            m_GtkimageStoreMain.GetIterFirst(out tree_iter);

            if (m_page_sizes.Count < 1)
                return;

            for (int k = 0; k < m_numpages; k++)
			{
				var doc_page = m_docPages[k];
				if (Math.Abs(doc_page.Zoom - m_doczoom) <= Single.Epsilon)
					continue;
				else
				{
                    /* Resize it now */
                    doc_page.PixBuf =
                        doc_page.PixBuf.ScaleSimple((int)(m_doczoom * m_page_sizes[k].width),
                        (int)(m_doczoom * m_page_sizes[k].height), Gdk.InterpType.Nearest);
                    doc_page.Width = (int)(m_doczoom * m_page_sizes[k].width);
					doc_page.Height = (int)(m_doczoom * m_page_sizes[k].height);
					doc_page.Zoom = m_doczoom;
					doc_page.Content= Page_Content_t.OLD_RESOLUTION;
                    m_GtkimageStoreMain.SetValue(tree_iter, 0, doc_page.PixBuf);
                    m_GtkimageStoreMain.IterNext(ref tree_iter);

                    if (k == 0)
                        m_page_scroll_pos[0] = doc_page.Height;
                    else
                        m_page_scroll_pos[k] = doc_page.Height + m_page_scroll_pos[k - 1];
                }
			}
		}
	}
}