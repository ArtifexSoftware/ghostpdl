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
using System.Runtime.InteropServices;   /* Marshaling */
using System.ComponentModel;            /* Background threading */
using System.Collections.Generic;       /* Use of List */
using System.IO;                        /* Use of path */
using GhostAPI;                         /* Use of Ghostscript API */

namespace GhostNET
{
	public enum GS_Task_t
	{
		PS_DISTILL,
		CREATE_XPS,
		SAVE_RESULT,
		GET_PAGE_COUNT,
		GENERIC,
		DISPLAY_DEV_THUMBS,
		DISPLAY_DEV_NON_PDF,
		DISPLAY_DEV_PDF,
		DISPLAY_DEV_RUN_FILE
	}
	public enum GS_Result_t
	{
		gsOK,
		gsFAILED,
		gsCANCELLED
	}
	public enum gsStatus
	{
		GS_READY,
		GS_BUSY,
		GS_ERROR
	};

	/* Parameters */
	public struct gsParamState_t
	{
		public String outputfile;
		public String inputfile;
		public GS_Task_t task;
		public GS_Result_t result;
		public int num_pages;
		public List<int> pages;
		public int firstpage;
		public int lastpage;
		public int currpage;
		public List<String> args;
		public int return_code;
		public double zoom;
		public bool aa;
		public bool is_valid;
	};

public class gsEventArgs : EventArgs
	{
		private bool m_completed;
		private int m_progress;
		private gsParamState_t m_param;
		public bool Completed
		{
			get { return m_completed; }
		}
		public gsParamState_t Params
		{
			get { return m_param; }
		}
		public int Progress
		{
			get { return m_progress; }
		}
		public gsEventArgs(bool completed, int progress, gsParamState_t param)
		{
			m_completed = completed;
			m_progress = progress;
			m_param = param;
		}
	}

	class GSNET
	{
		public class GhostscriptException : Exception
		{
			public GhostscriptException(string message) : base(message)
			{
			}
		}

		/* Ghostscript display device callback delegates. Make sure that
		   these are using Cdecl calling convention, which means gsdll
		   is doing the stack cleanup after the call */

		/* New device has been opened */
		/* This is the first event from this device. */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_open_del(IntPtr handle, IntPtr device);

		/* Device is about to be closed. */
		/* Device will not be closed until this function returns. */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_preclose_del(IntPtr handle, IntPtr device);

		/* Device has been closed. */
		/* This is the last event from this device. */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_close_del(IntPtr handle, IntPtr device);

		/* Device is about to be resized. */
		/* Resize will only occur if this function returns 0. */
		/* raster is byte count of a row. */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_presize_del(IntPtr handle, IntPtr device,
			int width, int height, int raster, uint format);

		/* Device has been resized. */
		/* New pointer to raster returned in pimage */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_size_del(IntPtr handle, IntPtr device,
							int width, int height, int raster, uint format,
							IntPtr pimage);

		/* flushpage */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_sync_del(IntPtr handle, IntPtr device);

		/* showpage */
		/* If you want to pause on showpage, then don't return immediately */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_page_del(IntPtr handle, IntPtr device, int copies, int flush);


		/* Notify the caller whenever a portion of the raster is updated. */
		/* This can be used for cooperative multitasking or for
		 * progressive update of the display.
		 * This function pointer may be set to NULL if not required.
		 */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_update_del(IntPtr handle, IntPtr device, int x, int y,
			  int w, int h);

		/* Allocate memory for bitmap */
		/* This is provided in case you need to create memory in a special
		 * way, e.g. shared.  If this is NULL, the Ghostscript memory device
		 * allocates the bitmap. This will only called to allocate the
		 * image buffer. The first row will be placed at the address
		 * returned by display_memalloc.
		 */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_memalloc_del(IntPtr handle, IntPtr device, ulong size);

		/* Free memory for bitmap */
		/* If this is NULL, the Ghostscript memory device will free the bitmap */
		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate int display_memfree_del(IntPtr handle, IntPtr device, IntPtr mem);

		private int display_size(IntPtr handle, IntPtr device,
							int width, int height, int raster, uint format,
							IntPtr pimage)
		{
			m_pagewidth = width;
			m_pageheight = height;
			m_pageraster = raster;
			m_pageptr = pimage;
			return 0;
		}

		private int display_page(IntPtr handle, IntPtr device, int copies, int flush)
		{
			m_params.currpage += 1;
			PageRenderedCallBack(m_pagewidth, m_pageheight, m_pageraster, m_pageptr, m_params);
			return 0;
		}

		private int display_open(IntPtr handle, IntPtr device)
		{
			return 0;
		}

		private int display_preclose(IntPtr handle, IntPtr device)
		{
			return 0;
		}

		private int display_close(IntPtr handle, IntPtr device)
		{
			return 0;
		}

		private int display_presize(IntPtr handle, IntPtr device,
			int width, int height, int raster, uint format)
		{
			return 0;
		}

		private int display_update(IntPtr handle, IntPtr device, int x, int y,
			  int w, int h)
		{
			return 0;
		}

		private int display_memalloc(IntPtr handle, IntPtr device, ulong size)
		{
			return 0;
		}

		private int display_memfree(IntPtr handle, IntPtr device, IntPtr mem)
		{
			return 0;
		}
		private int display_sync(IntPtr handle, IntPtr device)
		{
			return 0;
		}

		private int stdin_callback(IntPtr handle, IntPtr pointer, int count)
		{
			String output = Marshal.PtrToStringAnsi(pointer);
			return count;
		}

		private int stdout_callback(IntPtr handle, IntPtr pointer, int count)
		{
			String output = null;
			try
			{
				output = Marshal.PtrToStringAnsi(pointer);
			}
			catch (Exception except)
			{
				var mess = except.Message;
			}

			try
			{
				StdIOCallBack(output, count);
			}
			catch (Exception excep2)
			{
				var mess = excep2.Message;
			}

			/* Callback for progress on XPS creation */
			if (m_params.task == GS_Task_t.CREATE_XPS)
			{
				/* See if we have a page number */
				if (count >= 7 && output.Substring(0, 4) == "Page")
				{
					String page = output.Substring(5, count - 6);
					int numVal;
					try
					{
						double perc = 0.0;
						numVal = System.Convert.ToInt32(page);
						if (m_params.firstpage == -1 && m_params.lastpage == -1 &&
							m_params.pages == null)
						{
							/* Doing full document */
							perc = 100.0 * (double)numVal / (double)m_params.num_pages;
						}
						else
						{
							if (m_params.pages != null)
							{
								perc = 100.0 * ((double)numVal - m_params.currpage) / (double)m_params.num_pages;
								m_params.currpage = m_params.currpage + 1;
							}
							else
							{
								/* continugous set of pages */
								perc = 100.0 * ((double)numVal - m_params.firstpage + 1) / (double)m_params.num_pages;
							}
						}
						m_worker.ReportProgress((int)perc, m_params);
					}
					catch (FormatException)
					{
						Console.WriteLine("XPSPrint Error: Input string is not a sequence of digits.");
					}
					catch (OverflowException)
					{
						Console.WriteLine("XPSPrint Error: The number cannot fit in an Int32.");
					}
				}
			}
			return count;
		}

		private int stderr_callback(IntPtr handle, IntPtr pointer, int count)
		{
			String output = Marshal.PtrToStringAnsi(pointer);
			StdIOCallBack(output, count);
			return count;
		}

		IntPtr gsInstance;
		IntPtr dispInstance;
		BackgroundWorker m_worker;
		gsParamState_t m_params;
		IntPtr m_pageptr;
		int m_pagewidth;
		int m_pageheight;
		int m_pageraster;
		gsParamState_t m_displaystate;

		display_callback_t m_display_callback;
		IntPtr ptr_display_struct;

		/* Callbacks to application */
		internal delegate void DLLProblem(String mess);
		internal event DLLProblem DLLProblemCallBack;

		internal delegate void StdIO(String mess, int len);
		internal event StdIO StdIOCallBack;

		internal delegate void Progress(gsEventArgs info);
		internal event Progress ProgressCallBack;

		internal delegate void PageRendered(int width, int height, int raster,
			IntPtr data, gsParamState_t state);
		internal event PageRendered PageRenderedCallBack;

		/* From my understanding you cannot pin delegates.  These need to be declared
		 * as members to keep a reference to the delegates and avoid their possible GC. 
		 * since the C# GC has no idea that GS has a reference to these items. While the
		 * methods themselves don't move, apparently the address of the translation code
		 * is what is passed, and one of these abstractions can be GC or relocated. */
		GSAPI.gs_stdio_handler raise_stdin;
		GSAPI.gs_stdio_handler raise_stdout;
		GSAPI.gs_stdio_handler raise_stderr;
		
		/* Ghostscript display callback struct */
		public struct display_callback_t
		{
			public int sizeof_display_callback;
			public int major_vers;
			public int minor_vers;
			public display_open_del display_open;
			public display_preclose_del display_preclose;
			public display_close_del display_close;
			public display_presize_del display_presize;
			public display_size_del display_size;
			public display_sync_del display_sync;
			public display_page_del display_page;
			public display_update_del display_update;
			public display_memalloc_del display_memalloc;
			public display_memfree_del display_memfree;
		};

		public GSNET()
		{
			m_worker = null;
			gsInstance = IntPtr.Zero;
			dispInstance = IntPtr.Zero;

			/* To keep track if we need to update any parameters */
			m_displaystate = new gsParamState_t();
			m_displaystate.is_valid = false;

			/* Avoiding delegate GC during the life of this object */
			raise_stdin = stdin_callback;
			raise_stdout = stdout_callback;
			raise_stderr = stderr_callback;

			m_display_callback.major_vers = 1;
			m_display_callback.minor_vers = 0;
			m_display_callback.display_open = display_open;
			m_display_callback.display_preclose = display_preclose;
			m_display_callback.display_close = display_close;
			m_display_callback.display_presize = display_presize;
			m_display_callback.display_size = display_size;
			m_display_callback.display_sync = display_sync;
			m_display_callback.display_page = display_page;
			m_display_callback.display_update = display_update;
			//m_display_callback.display_memalloc = display_memalloc;
			//m_display_callback.display_memfree = display_memfree;
			m_display_callback.display_memalloc = null;
			m_display_callback.display_memfree = null;

			/* The size the structure when marshalled to unmanaged code */
			m_display_callback.sizeof_display_callback = Marshal.SizeOf(typeof(display_callback_t));

			ptr_display_struct = Marshal.AllocHGlobal(m_display_callback.sizeof_display_callback);
			Marshal.StructureToPtr(m_display_callback, ptr_display_struct, false);
		}

		/* Callback upon worker all done */
		private void gsCompleted(object sender, RunWorkerCompletedEventArgs e)
		{
			gsParamState_t Value;
			gsEventArgs info;
			gsParamState_t Params;

			try
			{
				Params = (gsParamState_t)e.Result;
			}
			catch (System.Reflection.TargetInvocationException)
			{
				/* Something went VERY wrong with GS */
				/* Following is to help debug these issues */
				/* var inner = ee.InnerException;
				var message = ee.Message;
				var inner_message = inner.Message;
				String bound = "\n************\n";
				gsIOUpdateMain(this, bound, bound.Length);
				gsIOUpdateMain(this, message, message.Length);
				gsIOUpdateMain(this, bound, bound.Length);
				gsIOUpdateMain(this, inner_message, inner_message.Length);
				gsIOUpdateMain(this, bound, bound.Length);
				var temp = inner.Source;
				gsIOUpdateMain(this, bound, bound.Length);
				gsIOUpdateMain(this, temp, temp.Length);
				var method = inner.TargetSite;
				gsIOUpdateMain(this, bound, bound.Length);
				var method_name = method.Name;
				gsIOUpdateMain(this, method_name, method_name.Length);
				var stack = inner.StackTrace;
				gsIOUpdateMain(this, bound, bound.Length);
				gsIOUpdateMain(this, stack, stack.Length); */
				String output = "Ghostscript DLL Invalid Access.";
				DLLProblemCallBack(output);
				return;
			}
			switch (Params.task)
			{
				case GS_Task_t.PS_DISTILL:
					m_worker.DoWork -= new DoWorkEventHandler(gsBytesAsync);
					break;
				case GS_Task_t.DISPLAY_DEV_NON_PDF:
				case GS_Task_t.DISPLAY_DEV_PDF:
				case GS_Task_t.DISPLAY_DEV_THUMBS:
					m_worker.DoWork -= new DoWorkEventHandler(DisplayDeviceAsync);
					break;
				case GS_Task_t.DISPLAY_DEV_RUN_FILE:
					m_worker.DoWork -= new DoWorkEventHandler(DisplayDeviceRun);
					break;
				default:
					m_worker.DoWork -= new DoWorkEventHandler(gsFileAsync);
					break;
			}

			if (e.Cancelled)
			{
				Value = new gsParamState_t();
				Value.result = GS_Result_t.gsCANCELLED;
				info = new gsEventArgs(true, 100, Value);
			}
			else
			{
				Value = (gsParamState_t)e.Result;
				info = new gsEventArgs(true, 100, Value);
			}
			ProgressCallBack(info);
		}

		/* Callback as worker progresses */
		private void gsProgressChanged(object sender, ProgressChangedEventArgs e)
		{
			/* Callback with progress */
			gsEventArgs info = new gsEventArgs(false, e.ProgressPercentage, (gsParamState_t) e.UserState);
			ProgressCallBack(info);
		}
		private gsParamState_t gsFileSync(gsParamState_t in_params)
		{
			int num_params = in_params.args.Count;
			var argParam = new GCHandle[num_params];
			var argPtrs = new IntPtr[num_params];
			List<byte[]> CharacterArray = new List<byte[]>(num_params);
			GCHandle argPtrsStable = new GCHandle();
			int code = 0;
			bool cleanup = true;

			try
			{
				code = GSAPI.gsapi_new_instance(out gsInstance, IntPtr.Zero);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileSync: gsapi_new_instance error");
				}
				code = GSAPI.gsapi_set_stdio(gsInstance, stdin_callback, stdout_callback, stderr_callback);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileSync: gsapi_set_stdio error");
				}
				code = GSAPI.gsapi_set_arg_encoding(gsInstance, (int)gsEncoding.GS_ARG_ENCODING_UTF8);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileSync: gsapi_set_arg_encoding error");
				}

				/* Now convert our Strings to char* and get pinned handles to these.
				 * This keeps the c# GC from moving stuff around on us */
				String fullcommand = "";
				for (int k = 0; k < num_params; k++)
				{
					CharacterArray.Add(System.Text.Encoding.UTF8.GetBytes((in_params.args[k]+"\0").ToCharArray()));
					argParam[k] = GCHandle.Alloc(CharacterArray[k], GCHandleType.Pinned);
					argPtrs[k] = argParam[k].AddrOfPinnedObject();
					fullcommand = fullcommand + " " + in_params.args[k];
				}

				/* Also stick the array of pointers into memory that will not be GCd */
				argPtrsStable = GCHandle.Alloc(argPtrs, GCHandleType.Pinned);

				fullcommand = "Command Line: " + fullcommand + "\n";
				StdIOCallBack(fullcommand, fullcommand.Length);
				code = GSAPI.gsapi_init_with_args(gsInstance, num_params, argPtrsStable.AddrOfPinnedObject());
				if (code < 0 && code != gsConstants.E_QUIT)
				{
					throw new GhostscriptException("gsFileSync: gsapi_init_with_args error");
				}
			}
			catch (DllNotFoundException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				in_params.result = GS_Result_t.gsFAILED;
				cleanup = false;
			}
			catch (BadImageFormatException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				in_params.result = GS_Result_t.gsFAILED;
				cleanup = false;
			}
			catch (GhostscriptException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			finally
			{
				/* All the pinned items need to be freed so the GC can do its job */
				if (cleanup)
				{
					for (int k = 0; k < num_params; k++)
					{
						argParam[k].Free();
					}
					argPtrsStable.Free();

					int code1 = GSAPI.gsapi_exit(gsInstance);
					if ((code == 0) || (code == gsConstants.E_QUIT))
						code = code1;

					GSAPI.gsapi_delete_instance(gsInstance);
					in_params.return_code = code;

					if ((code == 0) || (code == gsConstants.E_QUIT))
					{
						in_params.result = GS_Result_t.gsOK;
					}
					else
					{
						in_params.result = GS_Result_t.gsFAILED;
					}
					gsInstance = IntPtr.Zero;
				}
			}
			return in_params;
		}

		/* Process command line with gsapi_init_with_args */
		private void gsFileAsync(object sender, DoWorkEventArgs e)
		{
			gsParamState_t Params = (gsParamState_t)e.Argument;
			int num_params = Params.args.Count;
			var argParam = new GCHandle[num_params];
			var argPtrs = new IntPtr[num_params];
			List<byte[]> CharacterArray = new List<byte[]>(num_params);
			GCHandle argPtrsStable = new GCHandle();
			int code = 0;
			bool cleanup = true;

			try
			{
				code = GSAPI.gsapi_new_instance(out gsInstance, IntPtr.Zero);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileAsync: gsapi_new_instance error");
				}
				code = GSAPI.gsapi_set_stdio(gsInstance, stdin_callback, stdout_callback, stderr_callback);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileAsync: gsapi_set_stdio error");
				}
				code = GSAPI.gsapi_set_arg_encoding(gsInstance, (int)gsEncoding.GS_ARG_ENCODING_UTF8);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileAsync: gsapi_set_arg_encoding error");
				}

				/* Now convert our Strings to char* and get pinned handles to these.
				 * This keeps the c# GC from moving stuff around on us */
				String fullcommand = "";
				for (int k = 0; k < num_params; k++)
				{
					CharacterArray.Add(System.Text.Encoding.UTF8.GetBytes((Params.args[k]+"\0").ToCharArray()));
					argParam[k] = GCHandle.Alloc(CharacterArray[k], GCHandleType.Pinned);
					argPtrs[k] = argParam[k].AddrOfPinnedObject();
					fullcommand = fullcommand + " " + Params.args[k];
				}

				/* Also stick the array of pointers into memory that will not be GCd */
				argPtrsStable = GCHandle.Alloc(argPtrs, GCHandleType.Pinned);

				fullcommand = "Command Line: " + fullcommand + "\n";
				StdIOCallBack(fullcommand, fullcommand.Length);
				code = GSAPI.gsapi_init_with_args(gsInstance, num_params, argPtrsStable.AddrOfPinnedObject());
				if (code < 0)
				{
					throw new GhostscriptException("gsFileAsync: gsapi_init_with_args error");
				}
			}
			catch (DllNotFoundException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				Params.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = Params;
			}
			catch (BadImageFormatException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				Params.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = Params;
			}
			catch (GhostscriptException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			finally
			{ 
				if (cleanup)
				{
					/* All the pinned items need to be freed so the GC can do its job */
					for (int k = 0; k < num_params; k++)
					{
						argParam[k].Free();
					}
					argPtrsStable.Free();

					int code1 = GSAPI.gsapi_exit(gsInstance);
					if ((code == 0) || (code == gsConstants.E_QUIT))
						code = code1;

					GSAPI.gsapi_delete_instance(gsInstance);
					Params.return_code = code;

					if ((code == 0) || (code == gsConstants.E_QUIT))
					{
						Params.result = GS_Result_t.gsOK;
						e.Result = Params;
					}
					else
					{
						Params.result = GS_Result_t.gsFAILED;
						e.Result = Params;
					}
					gsInstance = IntPtr.Zero;
				}
			}
			return;
		}

		/* Processing with gsapi_run_string for callback progress */
		private void gsBytesAsync(object sender, DoWorkEventArgs e)
		{
			gsParamState_t Params = (gsParamState_t)e.Argument;
			int num_params = Params.args.Count;
			var argParam = new GCHandle[num_params];
			var argPtrs = new IntPtr[num_params];
			List<byte[]> CharacterArray = new List<byte[]>(num_params);
			GCHandle argPtrsStable = new GCHandle();
			Byte[] Buffer = new Byte[gsConstants.GS_READ_BUFFER];
			BackgroundWorker worker = sender as BackgroundWorker;
			int code = 0;
			int exitcode = 0;
			var Feed = new GCHandle();
			var FeedPtr = new IntPtr();
			String[] strParams = new String[num_params];
			FileStream fs = null;
			bool cleanup = true;

			try
			{
				/* Open the file */
				fs = new FileStream(Params.inputfile, FileMode.Open);
				var len = (int)fs.Length;

				code = GSAPI.gsapi_new_instance(out gsInstance, IntPtr.Zero);
				if (code < 0)
				{
					throw new GhostscriptException("gsBytesAsync: gsapi_new_instance error");
				}
				code = GSAPI.gsapi_set_stdio(gsInstance, stdin_callback, stdout_callback, stderr_callback);
				if (code < 0)
				{
					throw new GhostscriptException("gsBytesAsync: gsapi_set_stdio error");
				}
				code = GSAPI.gsapi_set_arg_encoding(gsInstance, (int)gsEncoding.GS_ARG_ENCODING_UTF8);
				if (code < 0)
				{
					throw new GhostscriptException("gsBytesAsync: gsapi_set_arg_encoding error");
				}

				/* Now convert our Strings to char* and get pinned handles to these.
				 * This keeps the c# GC from moving stuff around on us */
				String fullcommand = "";
				for (int k = 0; k < num_params; k++)
				{
					CharacterArray.Add(System.Text.Encoding.UTF8.GetBytes((Params.args[k]+"\0").ToCharArray()));
					argParam[k] = GCHandle.Alloc(CharacterArray[k], GCHandleType.Pinned);
					argPtrs[k] = argParam[k].AddrOfPinnedObject();
					fullcommand = fullcommand + " " + Params.args[k];
				}

				/* Also stick the array of pointers into memory that will not be GCd */
				argPtrsStable = GCHandle.Alloc(argPtrs, GCHandleType.Pinned);

				fullcommand = "Command Line: " + fullcommand + "\n";
				StdIOCallBack(fullcommand, fullcommand.Length);
				code = GSAPI.gsapi_init_with_args(gsInstance, num_params, argPtrsStable.AddrOfPinnedObject());
				if (code < 0)
				{
					throw new GhostscriptException("gsBytesAsync: gsapi_init_with_args error");
				}

				/* Pin data buffer */
				Feed = GCHandle.Alloc(Buffer, GCHandleType.Pinned);
				FeedPtr = Feed.AddrOfPinnedObject();

				/* Now start feeding the input piece meal and do a call back
				 * with our progress */
				if (code == 0)
				{
					int count;
					double perc;
					int total = 0;
					int ret_code;

					ret_code = GSAPI.gsapi_run_string_begin(gsInstance, 0, ref exitcode);
					if (exitcode < 0)
					{
						code = exitcode;
						throw new GhostscriptException("gsBytesAsync: gsapi_run_string_begin error");
					}

					while ((count = fs.Read(Buffer, 0, gsConstants.GS_READ_BUFFER)) > 0)
					{
						ret_code = GSAPI.gsapi_run_string_continue(gsInstance, FeedPtr, count, 0, ref exitcode);
						if (exitcode < 0)
						{
							code = exitcode;
							throw new GhostscriptException("gsBytesAsync: gsapi_run_string_continue error");
						}

						total = total + count;
						perc = 100.0 * (double)total / (double)len;
						worker.ReportProgress((int)perc, Params);
						if (worker.CancellationPending == true)
						{
							e.Cancel = true;
							break;
						}
					}
					ret_code = GSAPI.gsapi_run_string_end(gsInstance, 0, ref exitcode);
					if (exitcode < 0)
					{
						code = exitcode;
						throw new GhostscriptException("gsBytesAsync: gsapi_run_string_end error");
					}
				}
			}
			catch (DllNotFoundException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				Params.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = Params;
			}
			catch (BadImageFormatException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				Params.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = Params;
			}
			catch (GhostscriptException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			catch (Exception except)
			{
				/* Could be a file io issue */
				DLLProblemCallBack("Exception: " + except.Message);
				Params.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = Params;
			}
			finally
			{
				if (cleanup)
				{
					fs.Close();

					/* Free pinned items */
					for (int k = 0; k < num_params; k++)
					{
						argParam[k].Free();
					}
					argPtrsStable.Free();
					Feed.Free();

					/* gs clean up */
					int code1 = GSAPI.gsapi_exit(gsInstance);
					if ((code == 0) || (code == gsConstants.E_QUIT))
						code = code1;

					GSAPI.gsapi_delete_instance(gsInstance);
					Params.return_code = code;

					if ((code == 0) || (code == gsConstants.E_QUIT))
					{
						Params.result = GS_Result_t.gsOK;
						e.Result = Params;
					}
					else
					{
						Params.result = GS_Result_t.gsFAILED;
						e.Result = Params;
					}
					gsInstance = IntPtr.Zero;
				}
			}
			return;
		}

		private void SetPageRange(int first, int last, bool delay)
		{
			int code;

			byte[] param_first = System.Text.Encoding.UTF8.GetBytes("FirstPage" + "\0");
			byte[] param_last = System.Text.Encoding.UTF8.GetBytes("LastPage" + "\0");

			GCHandle pinParamFirst = GCHandle.Alloc(param_first, GCHandleType.Pinned);
			GCHandle pinParamLast = GCHandle.Alloc(param_last, GCHandleType.Pinned);
			GCHandle firstValue = GCHandle.Alloc(first, GCHandleType.Pinned);
			GCHandle lastValue = GCHandle.Alloc(last, GCHandleType.Pinned);
			code = GSAPI.gsapi_set_param(dispInstance, pinParamFirst.AddrOfPinnedObject(),
				firstValue.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int | gs_set_param_type.gs_spt_more_to_come);
			if (code < 0)
			{
				pinParamFirst.Free();
				pinParamLast.Free();
				firstValue.Free();
				lastValue.Free();
				throw new GhostscriptException("SetPageRange: gsapi_set_param error");
			}

			if (delay)
				code = GSAPI.gsapi_set_param(dispInstance, pinParamLast.AddrOfPinnedObject(),
					lastValue.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int | gs_set_param_type.gs_spt_more_to_come);
			else
				code = GSAPI.gsapi_set_param(dispInstance, pinParamLast.AddrOfPinnedObject(),
					lastValue.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int);

			pinParamFirst.Free();
			pinParamLast.Free();
			firstValue.Free();
			lastValue.Free();

			if (code < 0)
			{
				throw new GhostscriptException("SetPageRange: gsapi_set_param error");
			}
		}

		private void SetAA(bool aa, bool delay)
		{
			int code;
			int value;
			byte[] param_text = System.Text.Encoding.UTF8.GetBytes("TextAlphaBits" + "\0");
			byte[] param_graph = System.Text.Encoding.UTF8.GetBytes("GraphicsAlphaBits" + "\0");

			if (aa)
				value = 4;
			else
				value = 1;

			GCHandle pinParamText = GCHandle.Alloc(param_text, GCHandleType.Pinned);
			GCHandle pinParamGraph = GCHandle.Alloc(param_graph, GCHandleType.Pinned);
			GCHandle valueParam = GCHandle.Alloc(value, GCHandleType.Pinned);

			code = GSAPI.gsapi_set_param(dispInstance, pinParamText.AddrOfPinnedObject(),
				valueParam.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int | gs_set_param_type.gs_spt_more_to_come);
			if (code < 0)
			{
				pinParamText.Free();
				pinParamGraph.Free();
				valueParam.Free();
				throw new GhostscriptException("SetAA: gsapi_set_param error");
			}
			if (delay)
				code = GSAPI.gsapi_set_param(dispInstance, pinParamGraph.AddrOfPinnedObject(),
					valueParam.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int | gs_set_param_type.gs_spt_more_to_come);
			else
				code = GSAPI.gsapi_set_param(dispInstance, pinParamGraph.AddrOfPinnedObject(),
					valueParam.AddrOfPinnedObject(), gs_set_param_type.gs_spt_int);

			pinParamText.Free();
			pinParamGraph.Free();
			valueParam.Free();

			if (code < 0)
			{
				throw new GhostscriptException("SetAA: gsapi_set_param error");
			}
		}

		private void SetResolution(double zoom, bool delay)
		{
			int resolution = (int)(72.0 * zoom + 0.5);
			int code;

			byte[] param = System.Text.Encoding.UTF8.GetBytes("HWResolution" + "\0");
			byte[] value = System.Text.Encoding.UTF8.GetBytes("[" + resolution + " " + resolution + "]\0");
			GCHandle pinParam = GCHandle.Alloc(param, GCHandleType.Pinned);
			GCHandle valueParam = GCHandle.Alloc(value, GCHandleType.Pinned);

			if (delay)
				code = GSAPI.gsapi_set_param(dispInstance, pinParam.AddrOfPinnedObject(),
					valueParam.AddrOfPinnedObject(), gs_set_param_type.gs_spt_parsed | gs_set_param_type.gs_spt_more_to_come);
			else
				code = GSAPI.gsapi_set_param(dispInstance, pinParam.AddrOfPinnedObject(),
					valueParam.AddrOfPinnedObject(), gs_set_param_type.gs_spt_parsed);

			pinParam.Free();
			valueParam.Free();

			if (code < 0)
			{
				throw new GhostscriptException("SetResolution: gsapi_set_param error");
			}
		}

		/* Worker task for running a file with a page range, aa and zoom level */
		private void DisplayDeviceRun(object sender, DoWorkEventArgs e)
		{
			int code = 0;
			gsParamState_t gsparams = (gsParamState_t)e.Argument;
			int exitcode = 0;
			gsparams.result = GS_Result_t.gsOK;

			String fileName = gsparams.inputfile;
			int firstpage = gsparams.firstpage;
			int lastpage = gsparams.lastpage;

			gsparams.result = GS_Result_t.gsOK;

			if (dispInstance == IntPtr.Zero)
			{
				gsparams.result = GS_Result_t.gsFAILED;
				e.Result = gsparams;
				DLLProblemCallBack("Failure: Display device not initialized");
				return;
			}

			try
			{
				/* Set parameters if needed */
				if (!m_displaystate.is_valid)
				{
					/* First time after thumbnails */
					if (gsparams.aa)
					{
						SetAA(gsparams.aa, true);
					}

					if (!(gsparams.firstpage == -1 && gsparams.lastpage == -1))
					{
						SetResolution(gsparams.zoom, true);
						SetPageRange(gsparams.firstpage, gsparams.lastpage, false);
					}
					else
					{
						SetResolution(gsparams.zoom, false);
					}
					m_displaystate.is_valid = true;
				}
				else
				{
					/* Rendering page range */
					if (!(gsparams.firstpage == -1 && gsparams.lastpage == -1))
					{
						if (gsparams.aa != m_displaystate.aa)
							SetAA(gsparams.aa, true);
						if (gsparams.zoom != m_displaystate.zoom)
							SetResolution(gsparams.zoom, true);
						SetPageRange(gsparams.firstpage, gsparams.lastpage, false);
					}
					else
					{
						/* Rendering all pages */
						if (gsparams.aa != m_displaystate.aa)
						{
							if (gsparams.zoom != m_displaystate.zoom)
							{
								/* Zoom and AA change */
								SetAA(gsparams.aa, true);
								SetResolution(gsparams.zoom, false);
							}
							else
							{
								/* AA change only */
								SetAA(gsparams.aa, false);
							}
						}
						else
						{
							if (gsparams.zoom != m_displaystate.zoom)
							{
								/* Zoom change only */
								SetResolution(gsparams.zoom, false);
							}
						}
					}
				}
				m_displaystate.aa = gsparams.aa;
				m_displaystate.zoom = gsparams.zoom;

				byte[] fname = System.Text.Encoding.UTF8.GetBytes(fileName + "\0");
				GCHandle pinfname = GCHandle.Alloc(fname, GCHandleType.Pinned);
				code = GSAPI.gsapi_run_file(dispInstance, pinfname.AddrOfPinnedObject(), 0, ref exitcode);
				pinfname.Free();

				if (exitcode < 0)
				{
					throw new GhostscriptException("DisplayDeviceRun: gsapi_run_file error");
				}
			}
			catch (GhostscriptException except)
			{
				gsparams.result = GS_Result_t.gsFAILED;
				DLLProblemCallBack("Exception: " + except.Message);
			}
			finally
			{
				e.Result = gsparams;
			}
		}

		/* Worker task for using display device */
		private void DisplayDeviceAsync(object sender, DoWorkEventArgs e)
		{
			int code = 0;
			gsParamState_t gsparams = (gsParamState_t)e.Argument;
			GCHandle argPtrsStable = new GCHandle();

			int num_params = gsparams.args.Count;
			var argParam = new GCHandle[num_params];
			var argPtrs = new IntPtr[num_params];
			List<byte[]> CharacterArray = new List<byte[]>(num_params);
			bool cleanup = true;

			gsparams.result = GS_Result_t.gsOK;

			if (dispInstance == IntPtr.Zero)
			{
				gsparams.result = GS_Result_t.gsFAILED;
				e.Result = gsparams;
				DLLProblemCallBack("Failure: Display device not initialized");
				return;
			}

			try
			{

				String fullcommand = "";
				for (int k = 0; k < num_params; k++)
				{
					CharacterArray.Add(System.Text.Encoding.UTF8.GetBytes((gsparams.args[k] + "\0").ToCharArray()));
					argParam[k] = GCHandle.Alloc(CharacterArray[k], GCHandleType.Pinned);
					argPtrs[k] = argParam[k].AddrOfPinnedObject();
					fullcommand = fullcommand + " " + gsparams.args[k];
				}
				argPtrsStable = GCHandle.Alloc(argPtrs, GCHandleType.Pinned);

				fullcommand = "Command Line: " + fullcommand + "\n";
				StdIOCallBack(fullcommand, fullcommand.Length);
				code = GSAPI.gsapi_init_with_args(dispInstance, num_params, argPtrsStable.AddrOfPinnedObject());
				if (code < 0)
				{
					throw new GhostscriptException("DisplayDeviceAsync: gsapi_init_with_args error");
				}
			}

			catch (DllNotFoundException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = gsparams;
			}
			catch (BadImageFormatException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
				cleanup = false;
				e.Result = gsparams;
			}
			catch (GhostscriptException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
			}
			finally
			{
				if (cleanup)
				{
					for (int k = 0; k < num_params; k++)
					{
						argParam[k].Free();
					}
					argPtrsStable.Free();
					e.Result = gsparams;
				}
			}
			return;
		}

		/* Call the appropriate worker thread based upon the task
		 * that we have to do */
		private gsStatus RunGhostscriptAsync(gsParamState_t Params)
		{
			try
			{
				if (m_worker != null && m_worker.IsBusy)
				{
					m_worker.CancelAsync();
					return gsStatus.GS_BUSY;
				}
				if (m_worker == null)
				{
					m_worker = new BackgroundWorker();
					m_worker.WorkerReportsProgress = true;
					m_worker.WorkerSupportsCancellation = true;
					m_worker.RunWorkerCompleted += new RunWorkerCompletedEventHandler(gsCompleted);
					m_worker.ProgressChanged += new ProgressChangedEventHandler(gsProgressChanged);
				}

				switch (Params.task)
				{
					case GS_Task_t.PS_DISTILL:
						m_worker.DoWork += new DoWorkEventHandler(gsBytesAsync);
						break;
					case GS_Task_t.DISPLAY_DEV_NON_PDF:
					case GS_Task_t.DISPLAY_DEV_PDF:
					case GS_Task_t.DISPLAY_DEV_THUMBS:
						m_worker.DoWork += new DoWorkEventHandler(DisplayDeviceAsync);
						break;
					case GS_Task_t.DISPLAY_DEV_RUN_FILE:
						m_worker.DoWork += new DoWorkEventHandler(DisplayDeviceRun);
						break;
					case GS_Task_t.SAVE_RESULT:
					case GS_Task_t.CREATE_XPS:
					default:
						m_worker.DoWork += new DoWorkEventHandler(gsFileAsync);
						break;
				}

				m_params = Params;
				m_worker.RunWorkerAsync(Params);
				return gsStatus.GS_READY;
			}
			catch (OutOfMemoryException)
			{
				Console.WriteLine("Memory allocation failed during gs rendering\n");
				return gsStatus.GS_ERROR;
			}
		}

#region public_methods

		/* Direct call on gsapi to get the version of the DLL we are using */
		public String GetVersion()
		{
			gsapi_revision_t vers;
			vers.copyright = IntPtr.Zero;
			vers.product = IntPtr.Zero;
			vers.revision = 0;
			vers.revisiondate = 0;
			int size = System.Runtime.InteropServices.Marshal.SizeOf(vers);

			try
			{
				if (GSAPI.gsapi_revision(ref vers, size) == 0)
				{
					String product = Marshal.PtrToStringAnsi(vers.product);
					String output;
					int major = vers.revision / 100;
					int minor = vers.revision - major * 100;
					String versnum = major + "." + minor;
					output = product + " " + versnum;
					return output;
				}
				else
					return null;
			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
			}
			return null;
		}

		/* Use syncronous call to ghostscript to get the
		 * number of pages in a PDF file */
		public int GetPageCount(String fileName)
		{
			gsParamState_t gsparams = new gsParamState_t();
			gsParamState_t result;
			gsparams.args = new List<string>();

			gsparams.args.Add("gs");
			gsparams.args.Add("-dNODISPLAY");
			//gsparams.args.Add("-q");
			gsparams.args.Add("-sFile=\"" + fileName + "\"");
			gsparams.args.Add("--permit-file-read=\"" + fileName + "\"");
			gsparams.args.Add("-c");
			gsparams.args.Add("\"File (r) file runpdfbegin pdfpagecount = quit\"");
			gsparams.task = GS_Task_t.GET_PAGE_COUNT;
			m_params = gsparams;

			result = gsFileSync(gsparams);

			if (result.result == GS_Result_t.gsOK)
				return m_params.num_pages;
			else
				return -1;
		}

		/* Launch a thread to create XPS document for windows printing */
		public gsStatus CreateXPS(String fileName, int resolution, int num_pages,
								double width, double height, bool fit_page, int firstpage,
								int lastpage)
		{
			gsParamState_t gsparams = new gsParamState_t();
			gsparams.args = new List<string>();

			gsparams.inputfile = fileName;
			gsparams.args.Add("gs");
			gsparams.args.Add("-r" + resolution.ToString());
			gsparams.args.Add("-sDEVICE=xpswrite");
			gsparams.args.Add("-dNOPAUSE");
			gsparams.args.Add("-dBATCH");
			gsparams.args.Add("-dFirstPage=" + firstpage.ToString());
			gsparams.args.Add("-dLastPage=" + lastpage.ToString());

			gsparams.args.Add("-dDEVICEWIDTHPOINTS=" + Convert.ToInt32(width));
			gsparams.args.Add("-dDEVICEHEIGHTPOINTS=" + Convert.ToInt32(height));
			gsparams.args.Add("-dFIXEDMEDIA");

			if (fit_page)
				gsparams.args.Add("-dFitPage");

			gsparams.outputfile = Path.GetTempFileName();
			gsparams.args.Add("-o");
			gsparams.args.Add(gsparams.outputfile);
			gsparams.args.Add("-f");
			gsparams.args.Add(fileName);
			gsparams.task = GS_Task_t.CREATE_XPS;
			gsparams.num_pages = num_pages;

			return RunGhostscriptAsync(gsparams);
		}

		/* Launch a thread rendering all the pages with the display device
		 * to distill an input PS file and save as a PDF. */
		public gsStatus DistillPS(String fileName, int resolution)
		{
			gsParamState_t gsparams = new gsParamState_t();
			gsparams.args = new List<string>();

			gsparams.inputfile = fileName;
			gsparams.args.Add("gs");
			gsparams.args.Add("-sDEVICE=pdfwrite");
			gsparams.outputfile = Path.GetTempFileName();
			gsparams.args.Add("-o" + gsparams.outputfile);
			gsparams.task = GS_Task_t.PS_DISTILL;

			return RunGhostscriptAsync(gsparams);
		}

		/* Run a file with the display device 
		 * public gsStatus gsDsiplayDeviceRunFile() */
		public gsStatus DisplayDeviceRunFile(String fileName, double zoom, bool aa, int firstpage, int lastpage)
		{
			gsParamState_t gsparams = new gsParamState_t();

			gsparams.inputfile = fileName;
			gsparams.aa = aa;
			gsparams.zoom = zoom;
			gsparams.firstpage = firstpage;
			gsparams.lastpage = lastpage;
			if (firstpage > 0)
				gsparams.currpage = firstpage - 1;
			else
				gsparams.currpage = 0;

			gsparams.task = GS_Task_t.DISPLAY_DEV_RUN_FILE;

			return RunGhostscriptAsync(gsparams);
		}

		/* Launch a thread rendering all the pages with the display device
		 * to collect thumbnail images via init_with_args.   */
		public gsStatus DisplayDeviceRenderThumbs(String fileName, double zoom, bool aa)
		{
			gsParamState_t gsparams = new gsParamState_t();
			int format = (gsConstants.DISPLAY_COLORS_RGB |
							gsConstants.DISPLAY_DEPTH_8 |
							gsConstants.DISPLAY_LITTLEENDIAN);
			int resolution = (int)(72.0 * zoom + 0.5);
			GS_Task_t task = GS_Task_t.DISPLAY_DEV_THUMBS;

			gsparams.args = new List<string>();
			gsparams.args.Add("gs");
			gsparams.args.Add("-dFirstPage=1"); /* To ensure gdevflp is setup */
			gsparams.args.Add("-r" + resolution);
			if (aa)
			{
				gsparams.args.Add("-dTextAlphaBits=4");
				gsparams.args.Add("-dGraphicsAlphaBits=4");
			}
			gsparams.args.Add("-sDEVICE=display");
			gsparams.args.Add("-dDisplayFormat=" + format);
			gsparams.args.Add("-f");
			gsparams.args.Add(fileName);
			gsparams.task = task;
			gsparams.currpage = 0;

			return RunGhostscriptAsync(gsparams);
		}

		/* Launch a thread rendering a set of pages with the display device.  For use with languages
		 * that can be indexed via pages which include PDF and XPS */
		public gsStatus DisplayDeviceRenderPages(String fileName, int first_page, int last_page, double zoom)
		{
			gsParamState_t gsparams = new gsParamState_t();
			int format = (gsConstants.DISPLAY_COLORS_RGB |
							gsConstants.DISPLAY_DEPTH_8 |
							gsConstants.DISPLAY_LITTLEENDIAN);
			int resolution = (int)(72.0 * zoom + 0.5);

			gsparams.args = new List<string>();
			gsparams.args.Add("gs");
			gsparams.args.Add("-r" + resolution);
			gsparams.args.Add("-sDEVICE=display");
			gsparams.args.Add("-dDisplayFormat=" + format);
			gsparams.args.Add("-dFirstPage=" + first_page);
			gsparams.args.Add("-dLastPage=" + last_page);
			gsparams.args.Add("-f");
			gsparams.args.Add(fileName);
			gsparams.task = GS_Task_t.DISPLAY_DEV_PDF;
			gsparams.currpage = first_page - 1;

			return RunGhostscriptAsync(gsparams);
		}

		/* Set up the display device ahead of time */
		public gsParamState_t DisplayDeviceOpen()
		{
			int code;
			gsParamState_t gsparams = new gsParamState_t();
			gsparams.result = GS_Result_t.gsOK;

			if (dispInstance != IntPtr.Zero)
				return gsparams;

			try
			{
				code = GSAPI.gsapi_new_instance(out dispInstance, IntPtr.Zero);
				if (code < 0)
				{
					throw new GhostscriptException("DisplayDeviceOpen: gsapi_new_instance error");
				}

				code = GSAPI.gsapi_set_stdio(dispInstance, raise_stdin, raise_stdout, raise_stderr);
				if (code < 0)
				{
					throw new GhostscriptException("DisplayDeviceOpen: gsapi_set_stdio error");
				}

				code = GSAPI.gsapi_set_arg_encoding(dispInstance, (int)gsEncoding.GS_ARG_ENCODING_UTF8);
				if (code < 0)
				{
					throw new GhostscriptException("DisplayDeviceOpen: gsapi_set_arg_encoding error");
				}

				code = GSAPI.gsapi_set_display_callback(dispInstance, ptr_display_struct);
				if (code < 0)
				{
					throw new GhostscriptException("DisplayDeviceOpen: gsapi_set_display_callback error");
				}
			}

			catch (DllNotFoundException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
			}
			catch (BadImageFormatException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
			}
			catch (GhostscriptException except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
				if (dispInstance != IntPtr.Zero)
					GSAPI.gsapi_delete_instance(dispInstance);
				dispInstance = IntPtr.Zero;
			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				gsparams.result = GS_Result_t.gsFAILED;
				if (dispInstance != IntPtr.Zero)
					GSAPI.gsapi_delete_instance(dispInstance);
				dispInstance = IntPtr.Zero;
			}
			return gsparams;
		}

		/* Close the display device and delete the instance */
		public gsParamState_t DisplayDeviceClose()
		{
			int code = 0;
			gsParamState_t out_params = new gsParamState_t();

			out_params.result = GS_Result_t.gsOK;

			try
			{
				code = GSAPI.gsapi_exit(dispInstance);
				if (code < 0)
				{
					out_params.result = GS_Result_t.gsFAILED;
					throw new GhostscriptException("DisplayDeviceClose: gsapi_exit error");
				}

				GSAPI.gsapi_delete_instance(dispInstance);

			}
			catch (Exception except)
			{
				DLLProblemCallBack("Exception: " + except.Message);
				out_params.result = GS_Result_t.gsFAILED;
			}

			dispInstance = IntPtr.Zero;
			m_displaystate.is_valid = false;
			return out_params;
		}

		/* Check if gs is currently busy */
		public gsStatus GetStatus()
		{
			if (m_worker != null && m_worker.IsBusy)
				return gsStatus.GS_BUSY;
			else
				return gsStatus.GS_READY;
		}

		/* Cancel worker */
		public void Cancel()
		{
			m_worker.CancelAsync();
		}
#endregion
	}
}
