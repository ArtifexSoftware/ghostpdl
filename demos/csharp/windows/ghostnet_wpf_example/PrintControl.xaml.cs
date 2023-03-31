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
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Printing;
using System.Drawing.Printing;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;   /* DLLImport */
using System.Windows.Interop;

namespace ghostnet_wpf_example
{

	internal enum fModes
	{
		DM_SIZEOF = 0,
		DM_UPDATE = 1,
		DM_COPY = 2,
		DM_PROMPT = 4,
		DM_MODIFY = 8,
		DM_OUT_DEFAULT = DM_UPDATE,
		DM_OUT_BUFFER = DM_COPY,
		DM_IN_PROMPT = DM_PROMPT,
		DM_IN_BUFFER = DM_MODIFY,
	}

	/* Needed native methods for more advanced printing control */
	[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
	internal class PRINTER_INFO_9
	{
		/// <summary>
		/// A pointer to a DEVMODE structure that defines the per-user
		/// default printer data such as the paper orientation and the resolution.
		/// The DEVMODE is stored in the user's registry.
		/// </summary>
		public IntPtr pDevMode;
	}

	static class print_nativeapi
	{
		[DllImport("kernel32.dll")]
		public static extern IntPtr GlobalLock(IntPtr hMem);

		[DllImport("kernel32.dll")]
		[return: MarshalAs(UnmanagedType.Bool)]
		public static extern bool GlobalUnlock(IntPtr hMem);

		[DllImport("kernel32.dll")]
		public static extern IntPtr GlobalFree(IntPtr hMem);

		[DllImport("winspool.drv", SetLastError = true)]
		public static extern int OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);

		[DllImport("winspool.drv", SetLastError = true)]
		public static extern int SetPrinter(IntPtr phPrinter, UInt32 Level , IntPtr pPrinter, UInt32 Command);

		[DllImport("winspool.Drv", EntryPoint = "DocumentPropertiesW", SetLastError = true, ExactSpelling = true,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int DocumentProperties(IntPtr hwnd, IntPtr hPrinter, [MarshalAs(UnmanagedType.LPWStr)] string pDeviceName,
			IntPtr pDevModeOutput, IntPtr pDevModeInput, int fMode);

		[DllImport("winspool.drv", CharSet = CharSet.Unicode, SetLastError = true)]
		public static extern int DocumentProperties(IntPtr hWnd, IntPtr hPrinter, string pDeviceName, IntPtr pDevModeOutput,
			IntPtr pDevModeInput, fModes fMode);
	}

	static class NATIVEWIN
	{
		public const int IDOK = 1;
		public const int IDCANCEL = 2;
		public const int DM_OUT_BUFFER = 2;
		public const int DM_IN_BUFFER = 8;
		public const int DM_IN_PROMPT = 4;
		public const int DM_ORIENTATION = 1;
		public const int DM_PAPERSIZE = 2;
		public const int DM_PAPERLENGTH = 4;
		public const int DM_WIDTH = 8;
		public const int DMORIENT_PORTRAIT = 1;
		public const int DMORIENT_LANDSCAPE = 2;
	}
	public enum PrintPages_t
	{
		RANGE = 2,
		CURRENT = 1,
		ALL = 0
	}

	public enum PageScale_t
	{
		NONE = 0,
		FIT = 1,
	}

	public class PrintDiagEventArgs : EventArgs
	{
		public int m_page;

		public PrintDiagEventArgs(int page)
		{
			m_page = page;
		}
	}

	public class PrintRanges
	{
		public List<bool> ToPrint;
		public bool HasEvens;
		public bool HasOdds;
		public int NumberPages;

		public PrintRanges(int number_pages)
		{
			ToPrint = new List<bool>(number_pages);
			NumberPages = 0;
			HasEvens = false;
			HasOdds = false;
		}

		public void InitRange(Match match)
		{
			NumberPages = 0;
			HasEvens = false;
			HasOdds = false;

			for (int k = 0; k < ToPrint.Count; k++)
			{
				if (CheckValue(match, k))
				{
					NumberPages = NumberPages + 1;
					ToPrint[k] = true;
					if ((k + 1) % 2 != 0)
						HasOdds = true;
					else
						HasEvens = true;
				}
				else
					ToPrint[k] = false;
			}
		}

		private bool CheckValue(Match match, int k)
		{
			return false;
		}
	}

	public partial class Print : Window
	{
		private LocalPrintServer m_printServer;
		public PrintQueue m_selectedPrinter = null;
		String m_status;
		public PrintPages_t m_pages_setting;
		public double m_page_scale;
		int m_numpages;
		PrintCapabilities m_printcap;
		public PageSettings m_pagedetails;
		TranslateTransform m_trans_pap;
		TranslateTransform m_trans_doc;
		public bool m_isrotated;
		PrintRanges m_range_pages;
		public int m_numcopies;
		bool m_invalidTo;
		bool m_invalidFrom;
		public int m_from;
		public int m_to;
		ghostnet_wpf_example.MainWindow main;
		PrinterSettings m_ps;

		public Print(ghostnet_wpf_example.MainWindow main_in, int num_pages)
		{
			InitializeComponent();

			m_ps = new PrinterSettings();
			main = main_in;

			this.Closing += new System.ComponentModel.CancelEventHandler(FakeWindowClosing);
			InitializeComponent();
			m_printServer = new LocalPrintServer();
			m_selectedPrinter = LocalPrintServer.GetDefaultPrintQueue();
			m_ps.PrinterName = m_selectedPrinter.FullName;
			m_pagedetails = m_ps.DefaultPageSettings;


			xaml_rbAll.IsChecked = true;
			m_pages_setting = PrintPages_t.ALL;

			xaml_autofit.IsChecked = false;

			m_numpages = num_pages;

			m_printcap = m_selectedPrinter.GetPrintCapabilities();

			m_trans_pap = new TranslateTransform(0, 0);
			m_trans_doc = new TranslateTransform(0, 0);
			m_isrotated = false;

			/* Data range case */
			m_range_pages = new PrintRanges(m_numpages);
			m_page_scale = 1.0;

			m_numcopies = 1;

			m_invalidTo = true;
			m_invalidFrom = true;
			m_from = -1;
			m_to = -1;

			InitPrinterList();
		}
		void FakeWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			e.Cancel = true;
			this.Hide();
		}

		public void RealWindowClosing()
		{
			this.Closing -= new System.ComponentModel.CancelEventHandler(FakeWindowClosing);
			this.Close();
		}

		private void InitPrinterList()
		{
			PrintQueueCollection printQueuesOnLocalServer =
				m_printServer.GetPrintQueues(new[] { EnumeratedPrintQueueTypes.Local, EnumeratedPrintQueueTypes.Connections });

			this.xaml_selPrinter.ItemsSource = printQueuesOnLocalServer;
			if (m_selectedPrinter != null)
			{
				foreach (PrintQueue pq in printQueuesOnLocalServer)
				{
					if (pq.FullName == m_selectedPrinter.FullName)
					{
						this.xaml_selPrinter.SelectedItem = pq;
						break;
					}
				}
			}
		}

		/* Printer Status */
		private void GetPrinterStatus()
		{
			if (m_selectedPrinter != null)
			{
				if (m_selectedPrinter.IsBusy)
					m_status = "Busy";
				else if (m_selectedPrinter.IsNotAvailable)
					m_status = "Not Available";
				else if (m_selectedPrinter.IsOffline)
					m_status = "Offline";
				else if (m_selectedPrinter.IsOutOfMemory)
					m_status = "Out Of Memory";
				else if (m_selectedPrinter.IsOutOfPaper)
					m_status = "Out Of Paper";
				else if (m_selectedPrinter.IsOutputBinFull)
					m_status = "Output Bin Full";
				else if (m_selectedPrinter.IsPaperJammed)
					m_status = "Paper Jam";
				else if (m_selectedPrinter.IsPaused)
					m_status = "Paused";
				else if (m_selectedPrinter.IsPendingDeletion)
					m_status = "Paused";
				else if (m_selectedPrinter.IsPrinting)
					m_status = "Printing";
				else if (m_selectedPrinter.IsProcessing)
					m_status = "Processing";
				else if (m_selectedPrinter.IsWaiting)
					m_status = "Waiting";
				else if (m_selectedPrinter.IsWarmingUp)
					m_status = "Warming Up";
				else
					m_status = "Ready";
				xaml_Status.Text = m_status;
			}
		}

		private void selPrinterChanged(object sender, SelectionChangedEventArgs e)
		{
			m_selectedPrinter = this.xaml_selPrinter.SelectedItem as PrintQueue;
			GetPrinterStatus();
			m_ps.PrinterName = m_selectedPrinter.FullName;
			m_pagedetails = m_ps.DefaultPageSettings;
		}

		/* We have to do some calling into native methods to deal with and show
		 * the advanced properties referenced by the DEVMODE struture.  Ugly,
		 * but I could not figure out how to do with direct WPF C# methods */
		private void ShowProperties(object sender, RoutedEventArgs e)
		{
			try
			{
				/* First try to open the printer */
				IntPtr phPrinter;
				int result = print_nativeapi.OpenPrinter(m_ps.PrinterName, out phPrinter, IntPtr.Zero);
				if (result == 0)
				{
					return;
				}

				/* Get a pointer to the DEVMODE */
				IntPtr hDevMode = m_ps.GetHdevmode(m_ps.DefaultPageSettings);
				IntPtr pDevMode = print_nativeapi.GlobalLock(hDevMode);

				/* Native method wants a handle to our main window */
				IntPtr hwin = new WindowInteropHelper(this).Handle;

				/* Get size of DEVMODE */
				int sizeNeeded = print_nativeapi.DocumentProperties(hwin, IntPtr.Zero, m_ps.PrinterName, IntPtr.Zero, pDevMode, 0);

				/* Allocate */
				IntPtr devModeData = Marshal.AllocHGlobal(sizeNeeded);

				/* Get devmode and show properties window */
				print_nativeapi.DocumentProperties(hwin, IntPtr.Zero, m_ps.PrinterName, devModeData, pDevMode, fModes.DM_IN_PROMPT | fModes.DM_OUT_BUFFER);

				/* Set the properties, 9 = PRINTER_INFO_9.  This was 
				   tricky to figure out how to do */
				PRINTER_INFO_9 info = new PRINTER_INFO_9();
				info.pDevMode = devModeData;
				IntPtr infoPtr = Marshal.AllocHGlobal(Marshal.SizeOf<PRINTER_INFO_9>());
				Marshal.StructureToPtr<PRINTER_INFO_9>(info, infoPtr, false);
				result = print_nativeapi.SetPrinter(phPrinter, 9, infoPtr, 0);

				/* Clean up */
				print_nativeapi.GlobalUnlock(hDevMode);
				print_nativeapi.GlobalFree(hDevMode);
				Marshal.FreeHGlobal(infoPtr);
				//Marshal.FreeHGlobal(devModeData);  /* NB: Freeing this causes bad things for some reason. */
			}
			catch (Exception except)
			{
				main.ShowMessage(NotifyType_t.MESS_ERROR, "Exception in native print interface:" + except.Message);
			}
		}

		private void AllPages(object sender, RoutedEventArgs e)
		{
			m_pages_setting = PrintPages_t.ALL;
		}

		private void CurrentPage(object sender, RoutedEventArgs e)
		{
			m_pages_setting = PrintPages_t.CURRENT;
		}

		private void UpdatePageRange()
		{
			PrintDiagEventArgs info = new PrintDiagEventArgs(m_from - 1);
		}
		public bool RangeOK()
		{
			if (m_pages_setting == PrintPages_t.ALL ||
				m_pages_setting == PrintPages_t.CURRENT)
				return true;

			if (!m_invalidFrom && !m_invalidTo &&
				m_pages_setting == PrintPages_t.RANGE &&
				m_to >= m_from && m_to > 0 && m_from > 0)
				return true;

			return false;
		}

		private void PageRange(object sender, RoutedEventArgs e)
		{
			m_pages_setting = PrintPages_t.RANGE;
			if (RangeOK())
			{
				UpdatePageRange();
			}
		}

		private void PreviewTextInputFrom(object sender, TextCompositionEventArgs e)
		{
			Regex regex = new Regex("[^0-9]+");
			bool ok = !regex.IsMatch(e.Text);

			if (!ok)
				m_invalidFrom = true;
			else
				m_invalidFrom = false;
		}

		private void PreviewTextInputTo(object sender, TextCompositionEventArgs e)
		{
			Regex regex = new Regex("[^0-9]+");
			bool ok = !regex.IsMatch(e.Text);

			if (!ok)
				m_invalidTo = true;
			else
				m_invalidTo = false;
		}
		private void FromChanged(object sender, TextChangedEventArgs e)
		{
			Regex regex = new Regex("[^0-9]+");
			TextBox tbox = (TextBox)sender;

			if (tbox.Text == "")
			{
				e.Handled = true;
				return;
			}

			/* Need to check it again.  back space does not cause PreviewTextInputFrom
			 * to fire */
			bool ok = !regex.IsMatch(tbox.Text);
			if (!ok)
				m_invalidFrom = true;
			else
				m_invalidFrom = false;

			if (m_invalidFrom)
			{
				xaml_pagestart.Text = "";
				e.Handled = true;
				m_from = -1;
			}
			else
			{
				m_from = System.Convert.ToInt32(xaml_pagestart.Text);
				if (m_from > m_numpages)
				{
					m_from = m_numpages;
					xaml_pagestart.Text = System.Convert.ToString(m_numpages);
				}
				if (m_from < 1)
				{
					m_from = 1;
					xaml_pagestart.Text = System.Convert.ToString(m_numpages);
				}
				if (!m_invalidFrom && !m_invalidTo &&
					m_pages_setting == PrintPages_t.RANGE &&
					m_to >= m_from)
				{
					UpdatePageRange();
				}
			}
		}

		private void ToChanged(object sender, TextChangedEventArgs e)
		{
			Regex regex = new Regex("[^0-9]+");
			TextBox tbox = (TextBox)sender;

			if (tbox.Text == "")
			{
				e.Handled = true;
				return;
			}

			/* Need to check it again.  back space does not cause PreviewTextInputTo
			 * to fire */
			bool ok = !regex.IsMatch(tbox.Text);
			if (!ok)
				m_invalidTo = true;
			else
				m_invalidTo = false;

			if (m_invalidTo)
			{
				xaml_pageend.Text = "";
				e.Handled = true;
				m_to = -1;
			}
			else
			{
				m_to = System.Convert.ToInt32(xaml_pageend.Text);
				if (m_to > m_numpages)
				{
					m_to = m_numpages;
					xaml_pageend.Text = System.Convert.ToString(m_numpages);
				}
				if (m_to < 1)
				{
					m_to = 1;
					xaml_pagestart.Text = System.Convert.ToString(m_numpages);
				}
				if (!m_invalidFrom && !m_invalidTo &&
					m_pages_setting == PrintPages_t.RANGE &&
					m_to >= m_from)
				{
					UpdatePageRange();
				}
			}
		}

		private void xaml_Copies_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
		{
			m_numcopies = (int)e.NewValue;
		}

		private void AutoFit_Checked(object sender, RoutedEventArgs e)
		{

		}

		private void ClickOK(object sender, RoutedEventArgs e)
		{
			main.PrintDiagPrint(this);
			this.Hide();
		}

		private void ClickCancel(object sender, RoutedEventArgs e)
		{
			this.Hide();
		}

		private void AutoFit_Unchecked(object sender, RoutedEventArgs e)
		{

		}
	}
}
