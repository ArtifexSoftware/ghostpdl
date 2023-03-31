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
using System.Windows;
using System.Windows.Controls;
using System.Text.RegularExpressions;
using System.Windows.Input;
using System.Windows.Media;

namespace ghostnet_wpf_example
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

			if (curr_zoom == ZoomSteps[k + 1] && direction > 0)
			{
				if (k + 1 < ZoomSteps.Length - 1)
					return ZoomSteps[k + 2];
				else
					return ZoomSteps[k + 1];
			}

			/* Handling decreasing zoom case.  Look at lower boundary */
			if (curr_zoom > ZoomSteps[k] && direction < 0)
				return ZoomSteps[k];

			if (curr_zoom == ZoomSteps[k] && direction < 0)
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
			if (m_viewer_state != ViewerState_t.DOC_OPEN)
				return true;
			else
				return false;
		}

		private void ZoomOut(object sender, RoutedEventArgs e)
		{
			if (ZoomDisabled())
				return;

			if (m_doczoom <=  Constants.ZOOM_MIN)
				return;

			m_doczoom = GetNextZoom(m_doczoom, -1);
			xaml_Zoomsize.Text = Math.Round(m_doczoom * 100.0).ToString();
			ResizePages();
			RenderMain();
		}

		private void ZoomIn(object sender, RoutedEventArgs e)
		{
			if (ZoomDisabled())
				return;

			if (m_doczoom >= Constants.ZOOM_MAX)
				return;

			m_doczoom = GetNextZoom(m_doczoom, 1);
			xaml_Zoomsize.Text = Math.Round(m_doczoom * 100.0).ToString();
			ResizePages();
			RenderMain();
		}

		private void ZoomTextChanged(object sender, TextChangedEventArgs e)
		{
			Regex regex = new Regex("[^0-9.]+");
			System.Windows.Controls.TextBox tbox =
				(System.Windows.Controls.TextBox)sender;

			if (tbox.Text == "")
			{
				e.Handled = true;
				return;
			}

			/* Need to check it again.  back space does not cause PreviewTextInputTo
			 * to fire */
			bool ok = !regex.IsMatch(tbox.Text);
			if (ok)
				m_validZoom = true;
			else
			{
				m_validZoom = false;
				tbox.Text = "";
			}
		}

		private void ZoomEnterClicked(object sender, System.Windows.Input.KeyEventArgs e)
		{
			if (!m_validZoom)
				return;

			if (e.Key == Key.Return)
			{
				if (ZoomDisabled())
					return;

				e.Handled = true;
				var desired_zoom = xaml_Zoomsize.Text;
				try
				{
					double zoom = (double)System.Convert.ToDouble(desired_zoom) / 100.0;
					if (zoom > Constants.ZOOM_MAX)
						zoom = Constants.ZOOM_MAX;
					if (zoom < Constants.ZOOM_MIN)
						zoom = Constants.ZOOM_MIN;

					m_doczoom = zoom;
					ResizePages();
					RenderMain();
					xaml_Zoomsize.Text = Math.Round(zoom * 100.0).ToString();
				}
				catch (FormatException)
				{
					xaml_Zoomsize.Text = "";
					Console.WriteLine("String is not a sequence of digits.");
				}
				catch (OverflowException)
				{
					xaml_Zoomsize.Text = "";
					Console.WriteLine("The number cannot fit");
				}
			}
		}

		private void ResizePages()
		{
			if (m_page_sizes.Count == 0)
				return;

			int offset = 0;

			/* Get scroll relative location */
			Decorator border = VisualTreeHelper.GetChild(xaml_PageList, 0) as Decorator;
			ScrollViewer scrollViewer = border.Child as ScrollViewer;
			double top_window = scrollViewer.VerticalOffset;
			double x_size = scrollViewer.ExtentHeight;

			m_viewer_state = ViewerState_t.RESIZING;
			for (int k = 0; k < m_numpages; k++)
			{
				var doc_page = m_docPages[k];

				if (doc_page.Zoom != m_doczoom ||
					doc_page.Width != (int)(m_doczoom * m_page_sizes[k].size.X) ||
					doc_page.Height != (int)(m_doczoom * m_page_sizes[k].size.Y))
				{
					/* Resize it now */
					doc_page.Width = (int)(m_doczoom * m_page_sizes[k].size.X);
					doc_page.Height = (int)(m_doczoom * m_page_sizes[k].size.Y);
					doc_page.Zoom = m_doczoom;
					doc_page.Content= Page_Content_t.OLD_RESOLUTION;
				}

				/* Adjust page top locations */
				m_toppage_pos[k] = offset + Constants.PAGE_VERT_MARGIN;
				offset += doc_page.Height + 2 * Constants.PAGE_VERT_MARGIN;
			}

			m_toppage_pos[m_numpages] = offset;
			m_viewer_state = ViewerState_t.DOC_OPEN;

			/* Reset scroll location */
			/* This could probably be improved a bit.  We should also do horizontal position */
			scrollViewer.ScrollToVerticalOffset((top_window / x_size) * (double)offset);
		}
	}
}