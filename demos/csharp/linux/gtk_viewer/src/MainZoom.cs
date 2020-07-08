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
			if (!m_init_done || m_busy_render)
				return true;
			else
				return false;
		}

		private void ZoomOut(object sender)
		{
			if (ZoomDisabled())
				return;
			if (!m_init_done || m_doczoom <=  Constants.ZOOM_MIN)
				return;

			m_doczoom = GetNextZoom(m_doczoom, -1);
			//xaml_Zoomsize.Text = Math.Round(m_doczoom * 100.0).ToString();
			ResizePages();
			RenderMainAll();
		}

		private void ZoomIn(object sender)
		{
			if (ZoomDisabled())
				return;
			if (!m_init_done || m_doczoom >= Constants.ZOOM_MAX)
				return;

			m_doczoom = GetNextZoom(m_doczoom, 1);
			//xaml_Zoomsize.Text = Math.Round(m_doczoom * 100.0).ToString();
			ResizePages();
			RenderMainAll();
		}

		private void ZoomTextChanged(object sender)
		{
			Regex regex = new Regex("[^0-9.]+");
            //System.Windows.Controls.TextBox tbox =
            //	(System.Windows.Controls.TextBox)sender;
#if false
            if (tbox.Text == "")
			{
				e.Handled = true;
				return;
			}
#endif

/* Need to check it again.  back space does not cause PreviewTextInputTo
 * to fire */
#if false
            bool ok = !regex.IsMatch(tbox.Text);
			if (ok)
				m_validZoom = true;
			else
			{
				m_validZoom = false;
				//tbox.Text = "";
			}
#endif
		}

		private void ZoomEnterClicked(object sender)
		{
			if (!m_validZoom)
				return;
#if false
            if (e.Key == Key.Return)
			{
				e.Handled = true;
				//var desired_zoom = xaml_Zoomsize.Text;
				try
				{
					double zoom = (double)System.Convert.ToDouble(desired_zoom) / 100.0;
					if (zoom > Constants.ZOOM_MAX)
						zoom = Constants.ZOOM_MAX;
					if (zoom < Constants.ZOOM_MIN)
						zoom = Constants.ZOOM_MIN;

					m_doczoom = zoom;
					ResizePages();
					RenderMainAll();
					//xaml_Zoomsize.Text = Math.Round(zoom * 100.0).ToString();
				}
				catch (FormatException)
				{
					//xaml_Zoomsize.Text = "";
					Console.WriteLine("String is not a sequence of digits.");
				}
				catch (OverflowException)
				{
					//xaml_Zoomsize.Text = "";
					Console.WriteLine("The number cannot fit");
				}
			}
#endif
		}

		private void ResizePages()
		{
			if (m_page_sizes.Count == 0)
				return;

			for (int k = 0; k < m_numpages; k++)
			{
				var doc_page = m_docPages[k];
				if (doc_page.Zoom == m_doczoom &&
					doc_page.Width == (int)(m_doczoom * m_page_sizes[k].width) &&
					doc_page.Height == (int)(m_doczoom * m_page_sizes[k].height))
					continue;
				else
				{
					/* Resize it now */
					doc_page.Width = (int)(m_doczoom * m_page_sizes[k].width);
					doc_page.Height = (int)(m_doczoom * m_page_sizes[k].height);
					doc_page.Zoom = m_doczoom;
					doc_page.Content= Page_Content_t.OLD_RESOLUTION;
				}
			}
		}
	}
}