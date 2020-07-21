package com.artifex.gsviewer;

import static com.artifex.gsjava.GSAPI.GS_ERROR_OK;
import static com.artifex.gsjava.GSAPI.GS_NULL;
import static com.artifex.gsjava.GSAPI.gsapi_delete_instance;
import static com.artifex.gsjava.GSAPI.gsapi_new_instance;
import static com.artifex.gsjava.GSAPI.gsapi_revision;
import static com.artifex.gsjava.GSAPI.gsapi_set_arg_encoding;
import static com.artifex.gsjava.GSAPI.gsapi_set_stdio_with_handle;

import java.awt.Dimension;
import java.io.File;
import java.io.FileNotFoundException;

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
			//code = gsapi_set_display_callback(instanceRef.value, displayCallback);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to display callback (code = " + code + ")");
				gsapi_delete_instance(instanceRef.value);
				instanceRef.value = GS_NULL;
				return;
			}

			// 22 0 236
			final File file = new File("tiger.eps");
			if (!file.exists())
				throw new FileNotFoundException(file.getAbsolutePath());

			Document doc = Document.loadFromFile(instanceRef.value, file);

		} else {
			System.err.println("Failed to create new instance");
			return;
		}
		gsapi_delete_instance(instanceRef.value);
		instanceRef.value = GS_NULL;
	}
}
