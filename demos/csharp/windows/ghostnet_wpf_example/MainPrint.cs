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
using System.Collections.Generic;
using System.Threading;
using System.Windows.Xps.Packaging;
using System.Printing;
using System.Windows.Input;
using System.IO;
using System.Diagnostics;
using System.ComponentModel;
using GhostNET;

namespace ghostnet_wpf_example
{
	public partial class MainWindow
	{
		Print m_printcontrol = null;
		private xpsprint m_xpsprint = null;
		PrintStatus m_printstatus = null;

		public void PrintDiagPrint(object PrintDiag)
		{
			bool print_all = false;
			int first_page = 1;
			int last_page = 1;

			if (!m_printcontrol.RangeOK())
				return;

			/* Create only the page range that is needed. */
			switch (m_printcontrol.m_pages_setting)
			{
				case PrintPages_t.RANGE:
					first_page = m_printcontrol.m_from;
					last_page = m_printcontrol.m_to;
					break;
				case PrintPages_t.CURRENT:
					first_page = m_currpage + 1;
					last_page = m_currpage + 1;
					break;
				case PrintPages_t.ALL:
					print_all = true;
					first_page = 1;
					last_page = m_numpages;
					break;
			}

			/* Show the progress bar dialog */
			m_printstatus = new PrintStatus();
			m_printstatus.Activate();
			m_printstatus.Show();
			string extension = System.IO.Path.GetExtension(m_currfile);

			/* If file is already xps then gs need not do this */
			/* We are doing this based on the extension but should do
			 * it based upon the content */
			if (!(String.Equals(extension.ToUpper(), "XPS") || String.Equals(extension.ToUpper(), "OXPS")))
			{
				m_printstatus.xaml_PrintProgress.Value = 0;

				/* Extract needed information from print dialog and pass to GSNET */
				if (m_printcontrol == null)
				{
					return;
				}
				else
				{
					double width;
					double height;
					bool fit_page = (m_printcontrol.xaml_autofit.IsChecked == true);

					if (m_printcontrol.m_pagedetails.Landscape == true)
					{
						height = m_printcontrol.m_pagedetails.PrintableArea.Width * 72.0 / 100.0;
						width = m_printcontrol.m_pagedetails.PrintableArea.Height * 72.0 / 100.0;
					}
					else
					{
						height = m_printcontrol.m_pagedetails.PrintableArea.Height * 72.0 / 100.0;
						width = m_printcontrol.m_pagedetails.PrintableArea.Width * 72.0 / 100.0;
					}

					if (m_ghostscript.CreateXPS(m_currfile, Constants.DEFAULT_GS_RES,
						last_page - first_page + 1, height, width, fit_page, first_page, last_page) == gsStatus.GS_BUSY)
					{
						ShowMessage(NotifyType_t.MESS_STATUS, "GS currently busy");
						return;
					}
				}
			}
			else
				PrintXPS(m_currfile, print_all, first_page, last_page, false);
		}

		/* Printing is achieved using xpswrite device in ghostscript and
		 * pushing that file through the XPS print queue.  Since we can
		 * only have one instance of gs, this occurs on a separate 
		 * process compared to the viewer. */
		private void PrintCommand(object sender, ExecutedRoutedEventArgs e)
		{
			if (m_viewer_state != ViewerState_t.DOC_OPEN)
				return;

			/* Launch new process */
			string path = System.Reflection.Assembly.GetExecutingAssembly().CodeBase;
			try
			{
				string Arguments = "\"" + m_currfile + "\"" + " print " + m_currpage + " " + m_numpages;
				Process.Start(path, Arguments);
			}
			catch (InvalidOperationException)
			{
				Console.WriteLine("InvalidOperationException");
			}
			catch (Win32Exception)
			{
				Console.WriteLine("Win32 Exception: There was an error in opening the associated file. ");
			}
			return;
		}

		private void Print(string FileName)
		{
			m_currfile = FileName;
			if (m_printcontrol == null)
			{
				m_printcontrol = new Print(this, m_numpages);
				m_printcontrol.Activate();
				m_printcontrol.Show();  /* Makes it modal */
			}
			else
				m_printcontrol.Show();
			return;
		}

		/* Do the actual printing on a different thread. This is an STA
		   thread due to the UI like commands that the XPS document
		   processing performs.  */
		private void PrintXPS(String file, bool print_all, int from, int to,
							bool istempfile)
		{
			Thread PrintThread = new Thread(MainWindow.PrintWork);
			PrintThread.SetApartmentState(ApartmentState.STA);

			var arguments = new List<object>();
			arguments.Add(file);
			arguments.Add(print_all);
			arguments.Add(from);
			arguments.Add(to);
			arguments.Add(m_printcontrol.m_selectedPrinter.FullName);
			arguments.Add(m_printcontrol);
			arguments.Add(istempfile);

			m_xpsprint = new xpsprint();
			m_xpsprint.PrintUpdate += PrintProgress;
			arguments.Add(m_xpsprint);

			m_printstatus.xaml_PrintProgressText.Text = "Printing...";
			m_printstatus.xaml_PrintProgressText.FontWeight = FontWeights.Bold;
			m_printstatus.xaml_PrintProgressGrid.Visibility = System.Windows.Visibility.Visible;
			m_printstatus.xaml_PrintProgress.Value = 0;
			m_viewer_state = ViewerState_t.PRINTING;
			PrintThread.Start(arguments);
		}

		/* The thread that is actually doing the print work */
		public static void PrintWork(object data)
		{
			List<object> genericlist = data as List<object>;
			String file = (String)genericlist[0];
			bool print_all = (bool)genericlist[1];
			int from = (int)genericlist[2];
			int to = (int)genericlist[3];
			String printer_name = (String)genericlist[4];
			Print printcontrol = (Print)genericlist[5];
			bool istempfile = (bool)genericlist[6];
			xpsprint xps_print = (xpsprint)genericlist[7];
			String filename = "";

			if (istempfile == true)
				filename = file;

			/* We have to get our own copy of the print queue for the thread */
			LocalPrintServer m_printServer = new LocalPrintServer();
			PrintQueue m_selectedPrinter = m_printServer.GetPrintQueue(printer_name);

			XpsDocument xpsDocument = new XpsDocument(file, FileAccess.Read);
			xps_print.Print(m_selectedPrinter, xpsDocument, printcontrol, print_all,
							from, to, filename, istempfile);

			/* Once we are done, go ahead and delete the file */
			if (istempfile == true)
				xpsDocument.Close();
			xps_print.Done();
		}

		private void PrintProgress(object printHelper, gsPrintEventArgs Information)
		{
			switch (Information.Status)
			{
				case PrintStatus_t.PRINT_ERROR:
				case PrintStatus_t.PRINT_CANCELLED:
				case PrintStatus_t.PRINT_READY:
				case PrintStatus_t.PRINT_DONE:
					System.Windows.Application.Current.Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Normal, new Action(() =>
					{
						if (File.Exists(Information.FileName))
							DeleteTempFile(Information.FileName);
						System.Windows.Application.Current.Shutdown();
					}));
					break;
				case PrintStatus_t.PRINT_BUSY:
					System.Windows.Application.Current.Dispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Normal, new Action(() =>
					{
						m_printstatus.xaml_PrintProgress.Value = 100.0 * (double)(Information.Page - Information.PageStart) / (double)Information.NumPages;
						if (Information.Page == Information.NumPages)
						{
							m_printstatus.xaml_PrintProgress.Visibility = System.Windows.Visibility.Collapsed;
						}
					}));
					break;
			}
		}
	}
}