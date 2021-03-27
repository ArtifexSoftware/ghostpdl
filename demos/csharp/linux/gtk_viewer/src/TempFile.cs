/* Copyright (C) 2020-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

using System;
using System.IO;

/* A class to help in the management of temp files */
namespace gs_mono_example
{
	class TempFile
	{
		private String m_filename;

		public TempFile(String Name)
		{
			m_filename = Name;
		}

		public String Filename
		{
			get { return m_filename; }
		}

		public void DeleteFile()
		{
			try
			{
				if (File.Exists(m_filename))
					File.Delete(m_filename);
			}
			catch (Exception)
			{
				throw;
			}
		}
	}
}