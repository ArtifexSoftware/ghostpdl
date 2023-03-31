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
using System.ComponentModel;
using System.Diagnostics;
using Microsoft.Win32;
using GhostNET;
using System.IO;
using System.Windows.Media;

static class Constants
{
	public const double SCALE_THUMB = 0.1;
	public const int BLANK_WIDTH = 17;
	public const int BLANK_HEIGHT = 22;
	public const int DEFAULT_GS_RES = 300;
	public const int PAGE_VERT_MARGIN = 10;
	public const int MAX_PRINT_PREVIEW_LENGTH = 250;
	public const int ZOOM_MAX = 4;
	public const double ZOOM_MIN = 0.25;
}

namespace ghostnet_wpf_example
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	/// 
	public enum NotifyType_t
	{
		MESS_STATUS,
		MESS_ERROR
	};

	public enum status_t
	{
		S_ISOK,
		E_FAILURE,
		E_OUTOFMEM,
		E_NEEDPASSWORD
	};

	public enum Page_Content_t
	{
		FULL_RESOLUTION = 0,
		THUMBNAIL,
		OLD_RESOLUTION,
		LOW_RESOLUTION,
		NOTSET,
		BLANK
	};

	public enum zoom_t
	{
		NO_ZOOM,
		ZOOM_IN,
		ZOOM_OUT
	}

	public enum doc_t
	{
		UNKNOWN,
		PDF,
		PS,
		PCL,
		PNG,
		EPS,
		JPG,
		TIF,
		XPS
	}

	public enum ViewerState_t
	{
		NO_FILE,
		OPENING,
		BUSY_RENDER,
		DOC_OPEN,
		DISTILLING,
		PRINTING,
		RESIZING
	}

	public struct idata_t
	{
		public int page_num;
		public Byte[] bitmap;
		public int height;
		public int width;
		public int raster;
		public double zoom;
	}

	public struct pagesizes_t
	{
		public Point size;
		public double cummulative_y;
	}

	public partial class MainWindow : Window
	{
		GSNET m_ghostscript;
		doc_t m_document_type;
		bool m_doc_type_has_page_access;
		String m_currfile;
		List<TempFile> m_tempfiles;
		String m_origfile;
		int m_currpage;
		gsOutput m_gsoutput;
		public int m_numpages;
		private static Pages m_docPages;
		private static double m_doczoom;
		public List<pagesizes_t> m_page_sizes;
		List<idata_t> m_list_thumb;
		List<idata_t> m_images_rendered;
		bool m_validZoom;
		bool m_aa;
		List<int> m_toppage_pos;
		int m_page_progress_count;
		ViewerState_t m_viewer_state;

		private static List<Page_Content_t> m_pageType;

		public void ShowMessage(NotifyType_t type, String Message)
		{
			if (type == NotifyType_t.MESS_ERROR)
			{
				MessageBox.Show(Message, "Error", MessageBoxButton.OK);
			}
			else
			{
				MessageBox.Show(Message, "Notice", MessageBoxButton.OK);
			}
		}
		public MainWindow()
		{
			InitializeComponent();
			this.Closing += new System.ComponentModel.CancelEventHandler(Window_Closing);

			/* Set up ghostscript calls for progress update */
			m_ghostscript = new GSNET();
			m_ghostscript.ProgressCallBack+= new GSNET.Progress(gsProgress);
			m_ghostscript.StdIOCallBack += new GSNET.StdIO(gsIO);
			m_ghostscript.DLLProblemCallBack += new GSNET.DLLProblem(gsDLL);
			m_ghostscript.DisplayDeviceOpen();

			m_currpage = 0;
			m_gsoutput = new gsOutput();
			m_gsoutput.Activate();
			m_tempfiles = new List<TempFile>();
			m_thumbnails = new List<DocPage>();
			m_docPages = new Pages();
			m_pageType = new List<Page_Content_t>();
			m_page_sizes = new List<pagesizes_t>();
			m_document_type = doc_t.UNKNOWN;
			m_doczoom = 1.0;
			m_viewer_state = ViewerState_t.NO_FILE;
			m_validZoom = true;
			m_doc_type_has_page_access = true;
			m_list_thumb = new List<idata_t>();
			m_images_rendered = new List<idata_t>();
			m_aa = true;

			xaml_PageList.AddHandler(Grid.DragOverEvent, new System.Windows.DragEventHandler(Grid_DragOver), true);
			xaml_PageList.AddHandler(Grid.DropEvent, new System.Windows.DragEventHandler(Grid_Drop), true);


			/* For case of opening another file, or launching a print process */
			string[] arguments = Environment.GetCommandLineArgs();
			if (arguments.Length == 3)
			{
				string filePath = arguments[1];
				string job = arguments[2];

				if (String.Equals(job, "open"))
					ProcessFile(filePath);
			}
			else if (arguments.Length == 5)
			{
				string filePath = arguments[1];
				string job = arguments[2];

				try
				{
					m_currpage = Int32.Parse(arguments[3]);
					m_numpages = Int32.Parse(arguments[4]);
				}
				catch (FormatException)
				{
					Console.WriteLine("Failure to parse print page info");
					Close();
				}

				/* Keep main window hidden if we are doing a print process */
				this.IsVisibleChanged += new System.Windows.DependencyPropertyChangedEventHandler(WindowVisible);
				m_viewer_state = ViewerState_t.PRINTING;
				Print(filePath);
			}
		}

		private void gsIO(String mess, int len)
		{
			m_gsoutput.Update(mess, len);
		}

		private void ShowGSMessage(object sender, RoutedEventArgs e)
		{
			m_gsoutput.Show();
		}

		private void gsDLL(String mess)
		{
			ShowMessage(NotifyType_t.MESS_STATUS, mess);
		}

		private void gsThumbRendered(int width, int height, int raster,
			IntPtr data, gsParamState_t state)
		{
			ThumbPageCallback(width, height, raster, state.zoom, state.currpage, data);
		}

		private void gsPageRendered(int width, int height, int raster,
			IntPtr data, gsParamState_t state)
		{
			MainPageCallback(width, height, raster, state.zoom, state.currpage, data);
		}

		private void gsProgress(gsEventArgs asyncInformation)
		{
			if (asyncInformation.Completed)
			{
				switch  (asyncInformation.Params.task)
				{

					case GS_Task_t.CREATE_XPS:
						m_printstatus.xaml_PrintProgress.Value = 100;
						m_printstatus.xaml_PrintProgressGrid.Visibility = System.Windows.Visibility.Collapsed;
						break;

					case GS_Task_t.PS_DISTILL:
						xaml_DistillProgress.Value = 100;
						xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
						break;

					case GS_Task_t.SAVE_RESULT:
						break;

					case GS_Task_t.DISPLAY_DEV_THUMBS:
						ThumbsDone();
						break;

					case GS_Task_t.DISPLAY_DEV_RUN_FILE:
					case GS_Task_t.DISPLAY_DEV_PDF:
					case GS_Task_t.DISPLAY_DEV_NON_PDF:
						RenderingDone();
						break;

				}
				if (asyncInformation.Params.result == GS_Result_t.gsFAILED)
				{
					switch (asyncInformation.Params.task)
					{
						case GS_Task_t.CREATE_XPS:
							ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to create XPS");
							break;

						case GS_Task_t.PS_DISTILL:
							ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to distill PS");
							break;

						case GS_Task_t.SAVE_RESULT:
							ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed to convert document");
							break;

						default:
							ShowMessage(NotifyType_t.MESS_STATUS, "Ghostscript failed.");
							break;

					}
					return;
				}
				GSResult(asyncInformation.Params);
			}
			else
			{
				switch (asyncInformation.Params.task)
				{
					case GS_Task_t.CREATE_XPS:
						m_printstatus.xaml_PrintProgress.Value = asyncInformation.Progress;
						break;

					case GS_Task_t.PS_DISTILL:
						this.xaml_DistillProgress.Value = asyncInformation.Progress;
						break;

					case GS_Task_t.SAVE_RESULT:
						break;
				}
			}
		}

		public void GSResult(gsParamState_t gs_result)
		{
			TempFile tempfile = null;

			if (gs_result.outputfile != null)
				tempfile = new TempFile(gs_result.outputfile);

			if (gs_result.result == GS_Result_t.gsCANCELLED)
			{
				xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
				if (tempfile != null)
				{
					try
					{
						tempfile.DeleteFile();
					}
					catch
					{
						ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
					}
				}
				return;
			}
			if (gs_result.result == GS_Result_t.gsFAILED)
			{
				xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
				ShowMessage(NotifyType_t.MESS_STATUS, "GS Failed Conversion");
				if (tempfile != null)
				{
					try
					{
						tempfile.DeleteFile();
					}
					catch
					{
						ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
					}
				}
				return;
			}
			switch (gs_result.task)
			{
				case GS_Task_t.CREATE_XPS:
					/* Always do print all from xps conversion as it will do
					 * the page range handling for us */
					/* Add file to temp file list */
					m_tempfiles.Add(tempfile);
					PrintXPS(gs_result.outputfile, true, -1, -1, true);
					break;

				case GS_Task_t.PS_DISTILL:
					xaml_DistillGrid.Visibility = System.Windows.Visibility.Collapsed;
					m_origfile = gs_result.inputfile;

					/* Save distilled result */
					SaveFileDialog dlg = new SaveFileDialog();
					dlg.Filter = "PDF file (*.pdf)|*.pdf";
					dlg.FileName = System.IO.Path.GetFileNameWithoutExtension(m_origfile) + ".pdf";
					if (dlg.ShowDialog() == true)
					{
						try
						{
							if (File.Exists(dlg.FileName))
							{
								File.Delete(dlg.FileName);
							}
							File.Copy(tempfile.Filename, dlg.FileName);
						}
						catch (Exception except)
						{
							ShowMessage(NotifyType_t.MESS_ERROR, "Exception Saving Distilled Result:" + except.Message);
						}

					}
					tempfile.DeleteFile();
					m_viewer_state = ViewerState_t.NO_FILE;
					break;

				case GS_Task_t.SAVE_RESULT:
					/* Don't delete file in this case as this was our output! */
					ShowMessage(NotifyType_t.MESS_STATUS, "GS Completed Conversion");
					break;
			}
		}

		private void OpenFileCommand(object sender, ExecutedRoutedEventArgs e)
		{
			OpenFile(sender, e);
		}

		private void CleanUp()
		{
			/* Collapse this stuff since it is going to be released */
			xaml_ThumbGrid.Visibility = System.Windows.Visibility.Collapsed;

			/* Clear out everything */
			if (m_docPages != null && m_docPages.Count > 0)
				m_docPages.Clear();
			if (m_pageType != null && m_pageType.Count > 0)
				m_pageType.Clear();
			if (m_thumbnails != null && m_thumbnails.Count > 0)
				m_thumbnails.Clear();
			if (m_toppage_pos != null && m_toppage_pos.Count > 0)
				m_toppage_pos.Clear();
			if (m_list_thumb != null && m_list_thumb.Count > 0)
				m_list_thumb.Clear();
			if (m_images_rendered != null && m_images_rendered.Count > 0)
				m_images_rendered.Clear();
			if (m_page_sizes != null && m_page_sizes.Count > 0)
				m_page_sizes.Clear();

			m_currfile = null;
			m_origfile = null;
			m_numpages = -1;
			m_doc_type_has_page_access = true;
			m_document_type = doc_t.UNKNOWN;
			m_origfile = null;
			CleanUpTempFiles();
			xaml_TotalPages.Text = "/ 0";
			xaml_currPage.Text = "0";
			CloseExtraWindows(false);

			m_ghostscript.PageRenderedCallBack -= new GSNET.PageRendered(gsPageRendered);
			m_ghostscript.DisplayDeviceClose();
			m_ghostscript.DisplayDeviceOpen();
			m_viewer_state = ViewerState_t.NO_FILE;

			/* Set vertical scroll to top position */
			Decorator border = VisualTreeHelper.GetChild(xaml_PageList, 0) as Decorator;
			ScrollViewer scrollViewer = border.Child as ScrollViewer;
			scrollViewer.ScrollToVerticalOffset(0);
			return;
		}

		private void CloseCommand(object sender, ExecutedRoutedEventArgs e)
		{
			if (m_viewer_state == ViewerState_t.DOC_OPEN)
				CleanUp();
		}

		private bool ReadyForOpen()
		{
			/* Check if gs is currently busy. If it is then don't allow a new
			 * file to be opened. They can cancel gs with the cancel button if
			 * they want */
			if (m_ghostscript.GetStatus() != gsStatus.GS_READY)
			{
				ShowMessage(NotifyType_t.MESS_STATUS, "GS busy. Cancel to open new file.");
				return false;
			}
			return true;
		}

		private void OpenFile(object sender, RoutedEventArgs e)
		{
			if (!ReadyForOpen())
				return;

			OpenFileDialog dlg = new OpenFileDialog();
			dlg.Filter = "Document Files(*.ps;*.eps;*.pdf;*.bin;*.xps;*.oxps;*.jpg;*.png;*.tif|*.ps;*.eps;*.pdf;*.bin;*.xps;*.oxps;*.jpg;*.png;*.tif|All files (*.*)|*.*";
			dlg.FilterIndex = 1;
			if (dlg.ShowDialog() == true)
				ProcessFile(dlg.FileName);
		}

		public void ProcessFile(String FileName)
		{
			/* Before we even get started check for issues */
			/* First check if file exists and is available */
			if (!System.IO.File.Exists(FileName))
			{
				ShowMessage(NotifyType_t.MESS_STATUS, "File not found!");
				return;
			}
			if (m_viewer_state == ViewerState_t.DOC_OPEN)
			{
				/* In this case, we want to go ahead and launch a new process
				 * handing it the filename */
				/* We need to get the location */
				string path = System.Reflection.Assembly.GetExecutingAssembly().CodeBase;
				try
				{
					string Arguments = FileName + " open";
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
			else if (m_viewer_state != ViewerState_t.NO_FILE)
				return;

			m_viewer_state = ViewerState_t.OPENING;

			/* If we have a ps or eps file then launch the distiller first
			 * and then we will get a temp pdf file which we will open.  This is done
			 * to demo both methods of doing callbacks from gs worker thread.  Either
			 * progress as we distill the stream for PS or page by page for PDF */
			string extension = System.IO.Path.GetExtension(FileName);

			/* We are doing this based on the extension but should do
			 * it based upon the content */
			switch (extension.ToUpper())
			{
				case ".PS":
					m_document_type = doc_t.PS;
					break;
				case ".EPS":
					m_document_type = doc_t.EPS;
					break;
				case ".PDF":
					m_document_type = doc_t.PDF;
					break;
				case ".XPS":
					m_document_type = doc_t.XPS;
					break;
				case ".OXPS":
					m_document_type = doc_t.XPS;
					break;
				case ".BIN":
					m_document_type = doc_t.PCL;
					break;
				case ".PNG":
					m_document_type = doc_t.PNG;
					break;
				case ".JPG":
					m_document_type = doc_t.JPG;
					break;
				case ".TIF":
					m_document_type = doc_t.TIF;
					break;
				default:
					{
						m_document_type = doc_t.UNKNOWN;
						ShowMessage(NotifyType_t.MESS_STATUS, "Unknown File Type");
						m_viewer_state = ViewerState_t.NO_FILE;
						return;
					}
			}
			if (m_document_type == doc_t.PCL ||
				m_document_type == doc_t.XPS ||
				m_document_type == doc_t.PS)
			{
				MessageBoxResult result = MessageBox.Show("Would you like to distill this file?", "ghostnet", MessageBoxButton.YesNoCancel);
				switch (result)
				{
					case MessageBoxResult.Yes:
						xaml_DistillProgress.Value = 0;
						m_viewer_state = ViewerState_t.DISTILLING;
						if (m_ghostscript.DistillPS(FileName, Constants.DEFAULT_GS_RES) == gsStatus.GS_BUSY)
						{
							ShowMessage(NotifyType_t.MESS_STATUS, "GS currently busy");
							return;
						}
						xaml_DistillName.Text = "Distilling";
						xaml_CancelDistill.Visibility = System.Windows.Visibility.Visible;
						xaml_DistillName.FontWeight = FontWeights.Bold;
						xaml_DistillGrid.Visibility = System.Windows.Visibility.Visible;
						return;
					case MessageBoxResult.No:
						//m_doc_type_has_page_access = false;
						break;
					case MessageBoxResult.Cancel:
						m_viewer_state = ViewerState_t.NO_FILE;
						return;
				}
			}
			m_currfile = FileName;
			RenderThumbs();
			return;
		}

		private void CancelDistillClick(object sender, RoutedEventArgs e)
		{

		}

		private void DeleteTempFile(String file)
		{
			for (int k = 0; k < m_tempfiles.Count; k++)
			{
				if (String.Compare(file, m_tempfiles[k].Filename) == 0)
				{
					try
					{
						m_tempfiles[k].DeleteFile();
						m_tempfiles.RemoveAt(k);
					}
					catch
					{
						ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
					}
					break;
				}
			}
		}

		private void CleanUpTempFiles()
		{
			for (int k = 0; k < m_tempfiles.Count; k++)
			{
				try
				{
					m_tempfiles[k].DeleteFile();
				}
				catch
				{
					ShowMessage(NotifyType_t.MESS_STATUS, "Problem Deleting Temp File");
				}
			}
			m_tempfiles.Clear();
		}

		private void OnAboutClick(object sender, RoutedEventArgs e)
		{
			About about = new About(this);
			var desc_static = about.Description;
			String desc;

			String gs_vers = m_ghostscript.GetVersion();
			if (gs_vers == null)
				desc = "\nGhostscript DLL: Not Found";
			else
				desc = "\nGhostscript DLL: Using " + gs_vers + " 64 bit\n";

			about.description.Text = desc;
			about.ShowDialog();
		}

		private static DocPage InitDocPage()
		{
			DocPage doc_page = new DocPage();

			doc_page.BitMap = null;
			doc_page.Height = Constants.BLANK_HEIGHT;
			doc_page.Width = Constants.BLANK_WIDTH;
			return doc_page;
		}

		private void ThumbSelected(object sender, MouseButtonEventArgs e)
		{
			var item = ((FrameworkElement)e.OriginalSource).DataContext as DocPage;

			if (item != null)
			{
				if (item.PageNum < 0)
					return;

				var obj = xaml_PageList.Items[item.PageNum - 1];
				xaml_PageList.ScrollIntoView(obj);
			}
		}

		void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			CloseExtraWindows(true);
		}

		void CloseExtraWindows(bool shutdown)
		{
			if (shutdown)
			{
				if (m_gsoutput != null)
					m_gsoutput.RealWindowClosing();
				if (m_printcontrol != null)
					m_printcontrol.RealWindowClosing();
			}
			else
			{
				if (m_gsoutput != null)
					m_gsoutput.Hide();
				if (m_printcontrol != null)
					m_printcontrol.Hide();
			}
		}

		private void PageScrollChanged(object sender, ScrollChangedEventArgs e)
		{
			e.Handled = true;

			if (m_viewer_state != ViewerState_t.DOC_OPEN)
				return;

			/* Find the pages that are visible.  */
			double top_window = e.VerticalOffset;
			double bottom_window = top_window + e.ViewportHeight;
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
							first_page = k;
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

			m_currpage = first_page;
			xaml_currPage.Text = (m_currpage + 1).ToString();

			if (m_doc_type_has_page_access)
				PageRangeRender(first_page, last_page);

			return;
		}
		private void Grid_DragOver(object sender, System.Windows.DragEventArgs e)
		{
			if (e.Data.GetDataPresent(System.Windows.DataFormats.FileDrop))
			{
				e.Effects = System.Windows.DragDropEffects.All;
			}
			else
			{
				e.Effects = System.Windows.DragDropEffects.None;
			}
			e.Handled = false;
		}
		
		/* Keep main window hidden if we are doing a print process */
		public void WindowVisible(object sender, DependencyPropertyChangedEventArgs e)
		{
			if (m_viewer_state == ViewerState_t.PRINTING)
			{
				this.Visibility = Visibility.Hidden;
			}
		}

		private void Grid_Drop(object sender, System.Windows.DragEventArgs e)
		{
			if (e.Data.GetDataPresent(System.Windows.DataFormats.FileDrop))
			{
				string[] docPath = (string[])e.Data.GetData(System.Windows.DataFormats.FileDrop);
				ProcessFile(String.Join("", docPath));
			}
		}
		private void PageEnterClicked(object sender, KeyEventArgs e)
		{
			if (e.Key == Key.Return)
			{
				e.Handled = true;
				var desired_page = xaml_currPage.Text;
				try
				{
					int page = System.Convert.ToInt32(desired_page);
					if (page > 0 && page < (m_numpages + 1))
					{
						var obj = xaml_PageList.Items[page - 1];
						xaml_PageList.ScrollIntoView(obj);
					}
				}
				catch (FormatException)
				{
					Console.WriteLine("String is not a sequence of digits.");
				}
				catch (OverflowException)
				{
					Console.WriteLine("The number cannot fit in an Int32.");
				}
			}
		}

		private void AA_uncheck(object sender, RoutedEventArgs e)
		{
			m_aa = false;
			RenderMain();
		}

		private void AA_check(object sender, RoutedEventArgs e)
		{
			m_aa = true;
			RenderMain();
		}
	}
}
