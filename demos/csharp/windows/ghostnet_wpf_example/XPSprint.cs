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
using System.Printing;
using System.Windows.Documents;
using System.Windows.Documents.Serialization;
using System.Windows.Media;
using System.Windows.Xps;
using System.Windows.Xps.Packaging;
using System.Drawing.Printing;
using System.Reflection;

namespace ghostnet_wpf_example
{
	public enum PrintStatus_t
	{
		PRINT_READY,
		PRINT_BUSY,
		PRINT_ERROR,
		PRINT_CANCELLED,
		PRINT_DONE
	};

	/* Class for handling async print progress callback */
	public class gsPrintEventArgs : EventArgs
	{
		private PrintStatus_t m_status;
		private bool m_completed;
		private int m_page;
		private int m_page_start;
		private int m_num_pages;
		private String m_filename;

		public String FileName
		{
			get { return m_filename; }
		}
		public PrintStatus_t Status
		{
			get { return m_status; }
		}

		public bool Completed
		{
			get { return m_completed; }
		}

		public int Page
		{
			get { return m_page; }
		}

		public int PageStart
		{
			get { return m_page_start; }
		}

		public int NumPages
		{
			get { return m_num_pages; }
		}

		public gsPrintEventArgs(PrintStatus_t status, bool completed, int page,
			int page_start, int num_pages, String filename)
		{
			m_completed = completed;
			m_status = status;
			m_page = page;
			m_page_start = page_start;
			m_num_pages = num_pages;
			m_filename = filename;
		}
	}

	/* A page range paginator to override the DocumentPaginator */
	public class GSDocumentPaginator : DocumentPaginator
	{
		private int _first;
		private int _last;
		private DocumentPaginator _basepaginator;
		public GSDocumentPaginator(DocumentPaginator inpaginator, int firstpage,
									int lastpage)
		{
			_first = firstpage - 1;
			_last = lastpage - 1;
			_basepaginator = inpaginator;

			_last = Math.Min(_last, _basepaginator.PageCount - 1);
		}

		/* Wrap fixed page to avoid exception of fixed page in fixed page */
		public override DocumentPage GetPage(int page_num)
		{
			var page = _basepaginator.GetPage(page_num + _first);

			var vis_cont = new ContainerVisual();
			if (page.Visual is FixedPage)
			{
				foreach (var child in ((FixedPage)page.Visual).Children)
				{
					var clone = (System.Windows.UIElement)child.GetType().GetMethod("MemberwiseClone",
						BindingFlags.Instance | BindingFlags.NonPublic).Invoke(child, null);

					var parentField = clone.GetType().GetField("_parent",
						BindingFlags.Instance | BindingFlags.NonPublic);
					if (parentField != null)
					{
						parentField.SetValue(clone, null);
						vis_cont.Children.Add(clone);
					}
				}
				return new DocumentPage(vis_cont, page.Size, page.BleedBox,
										page.ContentBox);
			}
			return page;
		}

		public override bool IsPageCountValid
		{
			get { return true; }
		}

		public override int PageCount
		{
			get
			{
				if (_first > _basepaginator.PageCount - 1)
					return 0;
				if (_first > _last)
					return 0;
				return _last - _first + 1;
			}
		}

		public override System.Windows.Size PageSize
		{
			get { return _basepaginator.PageSize; }
			set { _basepaginator.PageSize = value; }
		}

		public override IDocumentPaginatorSource Source
		{
			get { return _basepaginator.Source; }
		}

		public override void ComputePageCount()
		{
			base.ComputePageCount();
		}

		protected override void OnGetPageCompleted(GetPageCompletedEventArgs e)
		{
			base.OnGetPageCompleted(e);
		}
	}

	public class xpsprint
	{
		private XpsDocumentWriter m_docWriter = null;
		internal delegate void AsyncPrintCallBack(object printObject, gsPrintEventArgs info);
		internal event AsyncPrintCallBack PrintUpdate;
		private bool m_busy;
		private int m_num_pages;
		private int m_first_page;
		private String m_filename;

		public bool IsBusy()
		{
			return m_busy;
		}

		public xpsprint()
		{
			m_busy = false;
		}

		public void Done()
		{
			gsPrintEventArgs info = new gsPrintEventArgs(PrintStatus_t.PRINT_DONE,
					true, 0, 0, 0, this.m_filename);
			PrintUpdate(this, info);
			m_busy = false;
		}

		/* Main print entry point */
		public void Print(PrintQueue queu, XpsDocument xpsDocument, Print printcontrol,
			bool print_all, int from, int to, String filename, bool tempfile)
		{
			XpsDocumentWriter docwrite;
			FixedDocumentSequence fixedDocSeq = xpsDocument.GetFixedDocumentSequence();
			DocumentReference docReference = fixedDocSeq.References[0] ;
			FixedDocument doc = docReference.GetDocument(false);

			PrintTicket Ticket = SetUpTicket(queu, printcontrol, fixedDocSeq, tempfile);
			docwrite = GetDocWriter(queu);
			m_busy = true;
			m_filename = filename;
#if DISABLED_FOR_NOW
			docwrite.WritingPrintTicketRequired +=
			 new WritingPrintTicketRequiredEventHandler(PrintTicket);
#endif
			PrintPages(docwrite, doc, print_all, from, to, Ticket);
		}

		/* Set up the print ticket */
		private PrintTicket SetUpTicket(PrintQueue queue, Print printcontrol,
			FixedDocumentSequence fixdoc, bool tempfile)
		{
			PrintTicket Ticket = new PrintTicket();

			PageMediaSizeName name = PaperKindToPageMediaSize(printcontrol.m_pagedetails.PaperSize.Kind);
			PageMediaSize mediasize = new PageMediaSize(name, printcontrol.m_pagedetails.PaperSize.Width, printcontrol.m_pagedetails.PaperSize.Height);

			/* Media size */
			Ticket.PageMediaSize = mediasize;
			/* Scale to fit */
			Ticket.PageScalingFactor = (int)Math.Round(printcontrol.m_page_scale * 100.0);

			System.Windows.Size page_size = new System.Windows.Size(mediasize.Width.Value, mediasize.Height.Value);
			DocumentPaginator paginator = fixdoc.DocumentPaginator;
			paginator.PageSize = page_size;

			/* Copy Count */
			Ticket.CopyCount = printcontrol.m_numcopies;

			if (printcontrol.m_pagedetails.Landscape)
				Ticket.PageOrientation = PageOrientation.Landscape;
			else
				Ticket.PageOrientation = PageOrientation.Portrait;

			/* Orientation. If we had a tempfile, then gs did a conversion
			 * and adjusted everything for us.  Other wise we may need to
			 * rotate the document if it was xps to start with. */
			if (printcontrol.m_isrotated && !tempfile)
				if (printcontrol.m_pagedetails.Landscape)
					Ticket.PageOrientation = PageOrientation.Portrait;
				else
					Ticket.PageOrientation = PageOrientation.Landscape;

			System.Printing.ValidationResult result = queue.MergeAndValidatePrintTicket(queue.UserPrintTicket, Ticket);
			queue.UserPrintTicket = result.ValidatedPrintTicket;
			queue.Commit();
			return result.ValidatedPrintTicket;
		}

		/* Send it */
		private void PrintPages(XpsDocumentWriter xpsdw, FixedDocument fixdoc,
			bool print_all, int from, int to, PrintTicket Ticket)
		{
			m_docWriter = xpsdw;
			xpsdw.WritingCompleted +=
				new WritingCompletedEventHandler(AsyncCompleted);
			xpsdw.WritingProgressChanged +=
				new WritingProgressChangedEventHandler(AsyncProgress);

			DocumentPaginator paginator = fixdoc.DocumentPaginator;
			try
			{
				if (print_all)
				{
					m_first_page = 1;
					m_num_pages = paginator.PageCount;
					xpsdw.Write(paginator, Ticket);
				}
				else
				{
					/* Create an override paginator to pick only the pages we want */
					GSDocumentPaginator gspaginator =
						new GSDocumentPaginator(paginator, from, to);
					m_first_page = from;
					m_num_pages = paginator.PageCount;
					xpsdw.Write(gspaginator, Ticket);
				}
			}
			catch (Exception)
			{
				/* Something went wrong with this particular print driver
				 * simply notify the user and clean up everything */
				gsPrintEventArgs info = new gsPrintEventArgs(PrintStatus_t.PRINT_ERROR,
						false, 0, this.m_first_page, this.m_num_pages, this.m_filename);
				PrintUpdate(this, info);
				return;
			}
		}
		public void CancelAsync()
		{
			/* ick.  This does not work in windows 8. causes crash. */
			/* https://connect.microsoft.com/VisualStudio/feedback/details/778145/xpsdocumentwriter-cancelasync-cause-crash-in-win8 */
			m_docWriter.CancelAsync();
		}

		/* Done */
		private void AsyncCompleted(object sender, WritingCompletedEventArgs e)
		{
			PrintStatus_t status;

			if (e.Cancelled)
				status = PrintStatus_t.PRINT_CANCELLED;
			else if (e.Error != null)
				status = PrintStatus_t.PRINT_ERROR;
			else
				status = PrintStatus_t.PRINT_READY;

			if (PrintUpdate != null)
			{
				gsPrintEventArgs info = new gsPrintEventArgs(status, true, 0,
					this.m_first_page, this.m_num_pages, this.m_filename);
				PrintUpdate(this, info);
			}
			m_busy = false;
		}

		/* Do this update with each fixed document (page) that is handled */
		private void AsyncProgress(object sender, WritingProgressChangedEventArgs e)
		{
			if (PrintUpdate != null)
			{
				gsPrintEventArgs info = new gsPrintEventArgs(PrintStatus_t.PRINT_BUSY,
									false, e.Number, this.m_first_page, this.m_num_pages,
									this.m_filename);
				PrintUpdate(this, info);
			}
		}
#if DISABLED_FOR_NOW
		/* Print ticket handling. You can customize for PrintTicketLevel at
		   FixedDocumentSequencePrintTicket, FixedDocumentPrintTicket,
		   or FixedPagePrintTicket.  We may want to play around with this some */
		private void PrintTicket(Object sender, WritingPrintTicketRequiredEventArgs e)
		{
			if (e.CurrentPrintTicketLevel ==
					PrintTicketLevel.FixedDocumentSequencePrintTicket)
			{
				PrintTicket pts = new PrintTicket();
				e.CurrentPrintTicket = pts;
			}
		}
#endif
		/* Create the document write */
		private XpsDocumentWriter GetDocWriter(PrintQueue pq)
		{
			XpsDocumentWriter xpsdw = PrintQueue.CreateXpsDocumentWriter(pq);
			return xpsdw;
		}

		/* Two paths for designating printing = a pain in the ass.*/
		static PageMediaSizeName PaperKindToPageMediaSize(PaperKind paperKind)
		{
			switch (paperKind)
			{
				case PaperKind.Custom:
					return PageMediaSizeName.Unknown;
				case PaperKind.Letter:
					return PageMediaSizeName.NorthAmericaLetter;
				case PaperKind.Legal:
					return PageMediaSizeName.NorthAmericaLegal;
				case PaperKind.A4:
					return PageMediaSizeName.ISOA4;
				case PaperKind.CSheet:
					return PageMediaSizeName.NorthAmericaCSheet;
				case PaperKind.DSheet:
					return PageMediaSizeName.NorthAmericaDSheet;
				case PaperKind.ESheet:
					return PageMediaSizeName.NorthAmericaESheet;
				case PaperKind.LetterSmall:
					return PageMediaSizeName.Unknown;
				case PaperKind.Tabloid:
					return PageMediaSizeName.NorthAmericaTabloid;
				case PaperKind.Ledger:
					return PageMediaSizeName.Unknown;
				case PaperKind.Statement:
					return PageMediaSizeName.NorthAmericaStatement;
				case PaperKind.Executive:
					return PageMediaSizeName.NorthAmericaExecutive;
				case PaperKind.A3:
					return PageMediaSizeName.ISOA3;
				case PaperKind.A4Small:
					return PageMediaSizeName.Unknown;
				case PaperKind.A5:
					return PageMediaSizeName.ISOA5;
				case PaperKind.B4:
					return PageMediaSizeName.ISOB4;
				case PaperKind.B5:
					return PageMediaSizeName.Unknown;
				case PaperKind.Folio:
					return PageMediaSizeName.OtherMetricFolio;
				case PaperKind.Quarto:
					return PageMediaSizeName.NorthAmericaQuarto;
				case PaperKind.Standard10x14:
					return PageMediaSizeName.Unknown;
				case PaperKind.Standard11x17:
					return PageMediaSizeName.Unknown;
				case PaperKind.Note:
					return PageMediaSizeName.NorthAmericaNote;
				case PaperKind.Number9Envelope:
					return PageMediaSizeName.NorthAmericaNumber9Envelope;
				case PaperKind.Number10Envelope:
					return PageMediaSizeName.NorthAmericaNumber10Envelope;
				case PaperKind.Number11Envelope:
					return PageMediaSizeName.NorthAmericaNumber11Envelope;
				case PaperKind.Number12Envelope:
					return PageMediaSizeName.NorthAmericaNumber12Envelope;
				case PaperKind.Number14Envelope:
					return PageMediaSizeName.NorthAmericaNumber14Envelope;
				case PaperKind.DLEnvelope:
					return PageMediaSizeName.ISODLEnvelope;
				case PaperKind.C5Envelope:
					return PageMediaSizeName.ISOC5Envelope;
				case PaperKind.C3Envelope:
					return PageMediaSizeName.ISOC3Envelope;
				case PaperKind.C4Envelope:
					return PageMediaSizeName.ISOC4Envelope;
				case PaperKind.C6Envelope:
					return PageMediaSizeName.ISOC6Envelope;
				case PaperKind.C65Envelope:
					return PageMediaSizeName.ISOC6C5Envelope;
				case PaperKind.B4Envelope:
					return PageMediaSizeName.ISOB4Envelope;
				case PaperKind.B5Envelope:
					return PageMediaSizeName.ISOB5Envelope;
				case PaperKind.B6Envelope:
					return PageMediaSizeName.Unknown;
				case PaperKind.ItalyEnvelope:
					return PageMediaSizeName.OtherMetricItalianEnvelope;
				case PaperKind.MonarchEnvelope:
					return PageMediaSizeName.NorthAmericaMonarchEnvelope;
				case PaperKind.PersonalEnvelope:
					return PageMediaSizeName.NorthAmericaPersonalEnvelope;
				case PaperKind.USStandardFanfold:
					return PageMediaSizeName.Unknown;
				case PaperKind.GermanStandardFanfold:
					return PageMediaSizeName.NorthAmericaGermanStandardFanfold;
				case PaperKind.GermanLegalFanfold:
					return PageMediaSizeName.NorthAmericaGermanLegalFanfold;
				case PaperKind.IsoB4:
					return PageMediaSizeName.ISOB4;
				case PaperKind.JapanesePostcard:
					return PageMediaSizeName.JapanHagakiPostcard;
				case PaperKind.Standard9x11:
					return PageMediaSizeName.Unknown;
				case PaperKind.Standard10x11:
					return PageMediaSizeName.Unknown;
				case PaperKind.Standard15x11:
					return PageMediaSizeName.Unknown;
				case PaperKind.InviteEnvelope:
					return PageMediaSizeName.OtherMetricInviteEnvelope;
				case PaperKind.LetterExtra:
					return PageMediaSizeName.NorthAmericaLetterExtra;
				case PaperKind.LegalExtra:
					return PageMediaSizeName.NorthAmericaLegalExtra;
				case PaperKind.TabloidExtra:
					return PageMediaSizeName.NorthAmericaTabloidExtra;
				case PaperKind.A4Extra:
					return PageMediaSizeName.ISOA4Extra;
				case PaperKind.LetterTransverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.A4Transverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.LetterExtraTransverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.APlus:
					return PageMediaSizeName.Unknown;
				case PaperKind.BPlus:
					return PageMediaSizeName.Unknown;
				case PaperKind.LetterPlus:
					return PageMediaSizeName.NorthAmericaLetterPlus;
				case PaperKind.A4Plus:
					return PageMediaSizeName.OtherMetricA4Plus;
				case PaperKind.A5Transverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.B5Transverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.A3Extra:
					return PageMediaSizeName.ISOA3Extra;
				case PaperKind.A5Extra:
					return PageMediaSizeName.ISOA5Extra;
				case PaperKind.B5Extra:
					return PageMediaSizeName.ISOB5Extra;
				case PaperKind.A2:
					return PageMediaSizeName.ISOA2;
				case PaperKind.A3Transverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.A3ExtraTransverse:
					return PageMediaSizeName.Unknown;
				case PaperKind.JapaneseDoublePostcard:
					return PageMediaSizeName.JapanDoubleHagakiPostcard;
				case PaperKind.A6:
					return PageMediaSizeName.ISOA6;
				case PaperKind.JapaneseEnvelopeKakuNumber2:
					return PageMediaSizeName.JapanKaku2Envelope;
				case PaperKind.JapaneseEnvelopeKakuNumber3:
					return PageMediaSizeName.JapanKaku3Envelope;
				case PaperKind.JapaneseEnvelopeChouNumber3:
					return PageMediaSizeName.JapanChou3Envelope;
				case PaperKind.JapaneseEnvelopeChouNumber4:
					return PageMediaSizeName.JapanChou4Envelope;
				case PaperKind.LetterRotated:
					return PageMediaSizeName.NorthAmericaLetterRotated;
				case PaperKind.A3Rotated:
					return PageMediaSizeName.ISOA3Rotated;
				case PaperKind.A4Rotated:
					return PageMediaSizeName.ISOA4Rotated;
				case PaperKind.A5Rotated:
					return PageMediaSizeName.ISOA5Rotated;
				case PaperKind.B4JisRotated:
					return PageMediaSizeName.JISB4Rotated;
				case PaperKind.B5JisRotated:
					return PageMediaSizeName.JISB5Rotated;
				case PaperKind.JapanesePostcardRotated:
					return PageMediaSizeName.JapanHagakiPostcardRotated;
				case PaperKind.JapaneseDoublePostcardRotated:
					return PageMediaSizeName.JapanHagakiPostcardRotated;
				case PaperKind.A6Rotated:
					return PageMediaSizeName.ISOA6Rotated;
				case PaperKind.JapaneseEnvelopeKakuNumber2Rotated:
					return PageMediaSizeName.JapanKaku2EnvelopeRotated;
				case PaperKind.JapaneseEnvelopeKakuNumber3Rotated:
					return PageMediaSizeName.JapanKaku3EnvelopeRotated;
				case PaperKind.JapaneseEnvelopeChouNumber3Rotated:
					return PageMediaSizeName.JapanChou3EnvelopeRotated;
				case PaperKind.JapaneseEnvelopeChouNumber4Rotated:
					return PageMediaSizeName.JapanChou4EnvelopeRotated;
				case PaperKind.B6Jis:
					return PageMediaSizeName.JISB6;
				case PaperKind.B6JisRotated:
					return PageMediaSizeName.JISB6Rotated;
				case PaperKind.Standard12x11:
					return PageMediaSizeName.Unknown;
				case PaperKind.JapaneseEnvelopeYouNumber4:
					return PageMediaSizeName.JapanYou4Envelope;
				case PaperKind.JapaneseEnvelopeYouNumber4Rotated:
					return PageMediaSizeName.JapanYou4EnvelopeRotated;
				case PaperKind.Prc16K:
					return PageMediaSizeName.PRC16K;
				case PaperKind.Prc32K:
					return PageMediaSizeName.PRC32K;
				case PaperKind.Prc32KBig:
					return PageMediaSizeName.PRC32KBig;
				case PaperKind.PrcEnvelopeNumber1:
					return PageMediaSizeName.PRC1Envelope;
				case PaperKind.PrcEnvelopeNumber2:
					return PageMediaSizeName.PRC2Envelope;
				case PaperKind.PrcEnvelopeNumber3:
					return PageMediaSizeName.PRC3Envelope;
				case PaperKind.PrcEnvelopeNumber4:
					return PageMediaSizeName.PRC4Envelope;
				case PaperKind.PrcEnvelopeNumber5:
					return PageMediaSizeName.PRC5Envelope;
				case PaperKind.PrcEnvelopeNumber6:
					return PageMediaSizeName.PRC6Envelope;
				case PaperKind.PrcEnvelopeNumber7:
					return PageMediaSizeName.PRC7Envelope;
				case PaperKind.PrcEnvelopeNumber8:
					return PageMediaSizeName.PRC8Envelope;
				case PaperKind.PrcEnvelopeNumber9:
					return PageMediaSizeName.PRC9Envelope;
				case PaperKind.PrcEnvelopeNumber10:
					return PageMediaSizeName.PRC10Envelope;
				case PaperKind.Prc16KRotated:
					return PageMediaSizeName.PRC16KRotated;
				case PaperKind.Prc32KRotated:
					return PageMediaSizeName.PRC32KRotated;
				case PaperKind.Prc32KBigRotated:
					return PageMediaSizeName.Unknown;
				case PaperKind.PrcEnvelopeNumber1Rotated:
					return PageMediaSizeName.PRC1EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber2Rotated:
					return PageMediaSizeName.PRC2EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber3Rotated:
					return PageMediaSizeName.PRC3EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber4Rotated:
					return PageMediaSizeName.PRC4EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber5Rotated:
					return PageMediaSizeName.PRC5EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber6Rotated:
					return PageMediaSizeName.PRC6EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber7Rotated:
					return PageMediaSizeName.PRC7EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber8Rotated:
					return PageMediaSizeName.PRC8EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber9Rotated:
					return PageMediaSizeName.PRC9EnvelopeRotated;
				case PaperKind.PrcEnvelopeNumber10Rotated:
					return PageMediaSizeName.PRC10EnvelopeRotated;
				default:
					throw new ArgumentOutOfRangeException("paperKind");
			}
		}
	}
}
