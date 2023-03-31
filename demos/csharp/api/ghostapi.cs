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

using System;                           /* IntPtr */
using System.Runtime.InteropServices;   /* DLLImport */

namespace GhostAPI
{
	public struct gsapi_revision_t
	{
		public IntPtr product;
		public IntPtr copyright;
		public int revision;
		public int revisiondate;
	}

	public enum gs_set_param_type
	{ 
		gs_spt_invalid =   -1,
		gs_spt_null =		0,   /* void * is NULL */
		gs_spt_bool =		1,   /* void * is NULL (false) or non-NULL (true) */
		gs_spt_int =		2,   /* void * is a pointer to an int */
		gs_spt_float =		3,   /* void * is a float * */
		gs_spt_name =		4,   /* void * is a char * */
		gs_spt_string =		5,   /* void * is a char * */
		gs_spt_long =		6,   /* void * is a long * */
		gs_spt_i64 =		7,   /* void * is a int64_t * */
		gs_spt_size_t =		8,    /* void * is a size_t * */
		gs_spt_parsed =		9,   /* void * is a pointer to a char * to be parsed */
		gs_spt_more_to_come = 1 << 31
	};

	public enum gsEncoding
	{
		GS_ARG_ENCODING_LOCAL = 0,
		GS_ARG_ENCODING_UTF8 = 1,
		GS_ARG_ENCODING_UTF16LE = 2
	};

	static class gsConstants
	{
		public const int E_QUIT = -101;
		public const int GS_READ_BUFFER = 32768;
		public const int DISPLAY_UNUSED_LAST = (1 << 7);
		public const int DISPLAY_COLORS_RGB = (1 << 2);
		public const int DISPLAY_DEPTH_8 = (1 << 11);
		public const int DISPLAY_LITTLEENDIAN = (1 << 16);
		public const int DISPLAY_BIGENDIAN = (0 << 16);
	}

	class GSAPI
	{
#if MONO
	private const string lib_dll = "libgpdl.so";
#else
#if WIN64
#if !GHOSTPDL
		private const string lib_dll = "gsdll64.dll";
#else
		private const string lib_dll = "gpdldll64.dll";
#endif
#else
#if !GHOSTPDL
		private const string lib_dll = "gsdll32.dll";
#else
		private const string lib_dll = "gpdldll32.dll";
#endif
#endif
#endif
		/* Callback proto for stdio */
		public delegate int gs_stdio_handler(IntPtr caller_handle, IntPtr buffer, int len);

		/* Callback proto for poll function */
		public delegate int gsPollHandler(IntPtr caller_handle);

		/* Callout proto */
		public delegate int gsCallOut(IntPtr callout_handle, IntPtr device_name, int id, int size, IntPtr data);

		[DllImport(lib_dll, EntryPoint = "gsapi_revision", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_revision(ref gsapi_revision_t vers, int size);

		[DllImport(lib_dll, EntryPoint = "gsapi_new_instance", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_new_instance(out IntPtr pinstance,
			IntPtr caller_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_delete_instance", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern void gsapi_delete_instance(IntPtr instance);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_stdio_with_handle", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_stdio_with_handle(IntPtr instance,
			gs_stdio_handler stdin, gs_stdio_handler stdout, gs_stdio_handler stderr, IntPtr caller_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_stdio", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_stdio(IntPtr instance,
			gs_stdio_handler stdin, gs_stdio_handler stdout, gs_stdio_handler stderr);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_poll_with_handle", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_poll_with_handle(IntPtr instance, gsPollHandler pollfn,
			IntPtr caller_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_poll", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_poll(IntPtr instance, gsPollHandler pollfn);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_display_callback", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_display_callback(IntPtr pinstance, IntPtr caller_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_register_callout", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_register_callout(IntPtr instance, gsCallOut callout,
			IntPtr callout_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_deregister_callout", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_deregister_callout(IntPtr instance, gsCallOut callout,
			IntPtr callout_handle);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_arg_encoding", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_arg_encoding(IntPtr instance,
			int encoding);

		[DllImport(lib_dll, EntryPoint = "gsapi_get_default_device_list", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_get_default_device_list(IntPtr instance,
			ref IntPtr list, ref int listlen);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_default_device_list", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_default_device_list(IntPtr instance,
			IntPtr list, ref int listlen);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_string_begin", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_string_begin(IntPtr instance,
			int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_string_continue", CharSet = CharSet.Ansi,
		CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_string_continue(IntPtr instance,
			IntPtr command, int count, int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_string_end", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_string_end(IntPtr instance,
			int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_string_with_length", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_string_with_length(IntPtr instance, IntPtr command,
			uint length, int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_string", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_string(IntPtr instance, IntPtr command,
			int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_run_file", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_run_file(IntPtr instance, IntPtr filename,
			int usererr, ref int exitcode);

		[DllImport(lib_dll, EntryPoint = "gsapi_init_with_args", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_init_with_args(IntPtr instance, int argc,
			IntPtr argv);

		[DllImport(lib_dll, EntryPoint = "gsapi_exit", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_exit(IntPtr instance);

		[DllImport(lib_dll, EntryPoint = "gsapi_set_param", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_set_param(IntPtr instance, IntPtr param, IntPtr value,
			gs_set_param_type type);

		[DllImport(lib_dll, EntryPoint = "gsapi_get_param", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_get_param(IntPtr instance, IntPtr param, IntPtr value,
			gs_set_param_type type);

		[DllImport(lib_dll, EntryPoint = "gsapi_enumerate_params", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_enumerate_params(IntPtr instance, out IntPtr iter,
			out IntPtr key, IntPtr type);

		[DllImport(lib_dll, EntryPoint = "gsapi_add_control_path", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_add_control_path(IntPtr instance, int type, IntPtr path);

		[DllImport(lib_dll, EntryPoint = "gsapi_remove_control_path", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_remove_control_path(IntPtr instance, int type, IntPtr path);

		[DllImport(lib_dll, EntryPoint = "gsapi_purge_control_paths", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern void gsapi_purge_control_paths(IntPtr instance, int type);

		[DllImport(lib_dll, EntryPoint = "gsapi_activate_path_control", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern void gsapi_activate_path_control(IntPtr instance, int enable);

		[DllImport(lib_dll, EntryPoint = "gsapi_is_path_control_active", CharSet = CharSet.Ansi,
			CallingConvention = CallingConvention.StdCall)]
		public static extern int gsapi_is_path_control_active(IntPtr instance);



	}
}
