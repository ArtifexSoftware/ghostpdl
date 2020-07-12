package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.artifex.gsjava.GSAPI.Revision;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.LongReference;

public class Main {

	public static void main(String[] args) throws FileNotFoundException {
		final Revision revision = new Revision();
		gsapi_revision(revision, 0);
		System.out.println("Product: " + revision.getProduct());
		System.out.println("Copyright: " + revision.getCopyright());
		System.out.println("Revision: " + revision.revision);
		System.out.println("Revision Date: " + revision.revisionDate);
		System.out.println();

		LongReference instanceRef = new LongReference();
		int code = gsapi_new_instance(instanceRef, GS_NULL);
		System.out.println("gsapi_new_instance() returned code " + code);
		if (code == GS_ERROR_OK) {
			System.out.println("Instance address: 0x" + Long.toHexString(instanceRef.value));
			System.out.println();

			final StdIO stdio = new StdIO();

			code = gsapi_set_stdio_with_handle(instanceRef.value, stdio, stdio, stdio, GS_NULL);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to set stdio with handle (code = " + code + ")");
				gsapi_delete_instance(instanceRef.value);
				instanceRef.value = GS_NULL;
				return;
			}

			code = gsapi_set_arg_encoding(instanceRef.value, 1);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to set arg encoding (code = " + code + ")");
				gsapi_delete_instance(instanceRef.value);
				instanceRef.value = GS_NULL;
				return;
			}

			final DisplayCallback displayCallback = new StandardDisplayCallback();
			code = gsapi_set_display_callback(instanceRef.value, displayCallback);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to display callback (code = " + code + ")");
				gsapi_delete_instance(instanceRef.value);
				instanceRef.value = GS_NULL;
				return;
			}

			final int format = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_LITTLEENDIAN;

			final File file = new File("Hello.pdf");
			if (!file.exists())
				throw new FileNotFoundException(file.getAbsolutePath());

			final File ofile = new File("image.tiff");


			String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER",
					"-I%rom%Resource%/Init/",
					"-dBATCH", "-r72", "-sDEVICE=display",
					"-dDisplayFormat=" + format,
					"-f",
					file.getAbsolutePath() };

			/*String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER",
					"-I%rom%Resource%/Init/",
					"-dBATCH", "-r72", "-sDEVICE=tiff24nc", "-o", ofile.getAbsolutePath(),
					"-f",
					file.getAbsolutePath() };*/
			//final String[] gargs = { "gs", "-Z#", "-h" };
			System.out.println("args = " + Arrays.toString(gargs));
			code = gsapi_init_with_args(instanceRef.value, gargs);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to gsapi_init_with_args (code = " + code + ")");
			} else {
				System.out.println("gsapi_init_with_args success");
			}

		} else {
			System.err.println("Failed to create new instance");
			return;
		}
		gsapi_delete_instance(instanceRef.value);
		instanceRef.value = GS_NULL;
	}
}
