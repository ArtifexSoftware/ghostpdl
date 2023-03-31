/* Copyright (C) 2023 Artifex Software, Inc.
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
using System.Diagnostics;
using GhostAPI;                         /* Use of Ghostscript API */
using System.Runtime.InteropServices;   /* Marshaling */
using System.Collections.Generic;       /* Use of List */

namespace ghostnet_commandline_app
{
	public class GhostscriptException : Exception
	{
		public GhostscriptException(string message) : base(message)
		{
		}
	}

	public enum GS_Result_t
	{
		gsOK,
		gsFAILED,
		gsCANCELLED
	}

	class ghostnetcl
	{
		/* From my understanding you cannot pin delegates.  These need to be declared
		 * as members to keep a reference to the delegates and avoid their possible GC.
		 * since the C# GC has no idea that GS has a reference to these items. While the
		 * methods themselves don't move, apparently the address of the translation code
		 * is what is passed, and one of these abstractions can be GC or relocated. */
		static GSAPI.gs_stdio_handler raise_stdin;
		static GSAPI.gs_stdio_handler raise_stdout;
		static GSAPI.gs_stdio_handler raise_stderr;

		static void Main(string[] args)
		{
			/* Avoiding delegate GC during the life of this object */
			raise_stdin = stdin_callback;
			raise_stdout = stdout_callback;
			raise_stderr = stderr_callback;

			Console.WriteLine("Welcome to the ghostnet command line application (x64 only).");
			Console.WriteLine("Type in the commmand line options you wish to execute.");
			Console.WriteLine("e.g. gs -sDEVICE=tiff24nc -r72 -o myoutput.tif -f myinput.pdf");

			while (true)
			{
				Console.Write("GS# > ");
				string input = Console.ReadLine();
				string[] tokens;

				if (input.Length == 0)
					continue;

				/* Check for the use of quotes. If none use
				   simple parser */
				int index = input.IndexOf("\"");

				if (index == -1)
					tokens = input.Split(' ', StringSplitOptions.RemoveEmptyEntries);
				else
				{
					tokens = parse(input);
					if (tokens == null)
						continue;
				}

				if (tokens[0] != "gs")
					continue;

				Stopwatch timing = new Stopwatch();
				timing.Start();

				try
				{
					GS_Result_t code = gsFileSync(tokens);
				}
				catch (Exception except)
				{
					var mess = except.Message;
				}

				timing.Stop();
				Console.WriteLine("\nElapsed Time={0}", timing.Elapsed);
			}
		}

		/* This was beat on a bit. It could have issues, but it is demo code */
		static private string[] parse(string input)
		{
			string[] tokens = new string[] { };
			int full_len = input.Length;
			int quote_start;
			int quote_end;
			int curr_start = 0;
			int curr_len;

			quote_start = input.IndexOf("\"");
			if (quote_start == full_len - 1)
				return null;
			quote_end = input.IndexOf("\"", quote_start + 1);
			if (quote_end == -1)
				return null;

			while (true)
			{
				if (quote_start != -1)
					curr_len = quote_start - curr_start;
				else
					curr_len = full_len - curr_start;

				/* Get stuff before quote */
				string[] tokens_curr = input.Substring(curr_start, curr_len).Split(' ', StringSplitOptions.RemoveEmptyEntries);
				int old_length = tokens.Length;
				Array.Resize(ref tokens, tokens.Length + tokens_curr.Length);
				tokens_curr.CopyTo(tokens, old_length);

				/* Now the quotes */
				if (quote_start != -1)
				{
					string quote_part = input.Substring(quote_start, quote_end - quote_start + 1);
					Array.Resize(ref tokens, tokens.Length + 1);
					tokens[tokens.Length - 1] = quote_part;

					/* Update next start location */
					curr_start = quote_end + 1;

					/* Update new quote locations */
					quote_start = input.IndexOf("\"", quote_end + 1);
					if (quote_start != -1)
						quote_end = input.IndexOf("\"", quote_start + 1);
					else
						quote_end = -1;

					/* Only one left ! */
					if (quote_end == -1 && quote_start != -1)
						return null;
				}
				else
				{
					/* No quotes. Go to end */
					curr_start = curr_start + curr_len;
					quote_end = -1;
				}

				/* Check if done */
				if (curr_start == full_len)
					break;
			}
			return tokens;
		}

		static private void dll_issue(string message)
		{
			Console.Write(message);
		}

		static private void gs_callback(string message, int count)
		{
			string partial_message = message.Substring(0, count);
			Console.Write(partial_message);
		}

		static private int stdin_callback(IntPtr handle, IntPtr pointer, int count)
		{
			String output = Marshal.PtrToStringAnsi(pointer);
			return count;
		}

		static private int stdout_callback(IntPtr handle, IntPtr pointer, int count)
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
				gs_callback(output, count);
			}
			catch (Exception excep2)
			{
				var mess = excep2.Message;
			}
			return count;
		}

		static private int stderr_callback(IntPtr handle, IntPtr pointer, int count)
		{
			String output = Marshal.PtrToStringAnsi(pointer);
			gs_callback(output, count);
			return count;
		}

		/* Code to run GS synchronously */
		private static GS_Result_t gsFileSync(string[] tokens)
		{
			int num_params = tokens.Length;
			var argParam = new GCHandle[num_params];
			var argPtrs = new IntPtr[num_params];
			List<byte[]> CharacterArray = new List<byte[]>(num_params);
			GCHandle argPtrsStable = new GCHandle();
			int code = 0;
			bool cleanup = true;
			IntPtr gsInstance = IntPtr.Zero;
			GS_Result_t result = GS_Result_t.gsOK;

			try
			{
				code = GSAPI.gsapi_new_instance(out gsInstance, IntPtr.Zero);
				if (code < 0)
				{
					throw new GhostscriptException("gsFileSync: gsapi_new_instance error");
				}
				code = GSAPI.gsapi_set_stdio(gsInstance, raise_stdin, raise_stdout, raise_stderr);
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
					CharacterArray.Add(System.Text.Encoding.UTF8.GetBytes((tokens[k] + "\0").ToCharArray()));
					argParam[k] = GCHandle.Alloc(CharacterArray[k], GCHandleType.Pinned);
					argPtrs[k] = argParam[k].AddrOfPinnedObject();
					fullcommand = fullcommand + " " + tokens[k];
				}

				/* Also stick the array of pointers into memory that will not be GCd */
				argPtrsStable = GCHandle.Alloc(argPtrs, GCHandleType.Pinned);

				fullcommand = "Command Line: " + fullcommand + "\n";
				code = GSAPI.gsapi_init_with_args(gsInstance, num_params, argPtrsStable.AddrOfPinnedObject());
				if (code < 0 && code != gsConstants.E_QUIT)
				{
					throw new GhostscriptException("gsFileSync: gsapi_init_with_args error");
				}
			}
			catch (DllNotFoundException except)
			{
				dll_issue("Exception: " + except.Message);
				result = GS_Result_t.gsFAILED;
				cleanup = false;
			}
			catch (BadImageFormatException except)
			{
				dll_issue("Exception: " + except.Message);
				result = GS_Result_t.gsFAILED;
				cleanup = false;
			}
			catch (GhostscriptException except)
			{
				dll_issue("Exception: " + except.Message);
			}
			catch (Exception except)
			{
				dll_issue("Exception: " + except.Message);
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
					if (code < 0)
						result = GS_Result_t.gsFAILED;

					if ((code == 0) || (code == gsConstants.E_QUIT))
					{
						result = GS_Result_t.gsOK;
					}
					else
					{
						result = GS_Result_t.gsFAILED;
					}
					gsInstance = IntPtr.Zero;
				}
			}
			return result;
		}
	}
}
