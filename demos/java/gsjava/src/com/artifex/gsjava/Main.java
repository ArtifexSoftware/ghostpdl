package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.util.ArrayList;
import java.util.List;

import com.artifex.gsjava.GSAPI.Revision;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.LongReference;
import com.artifex.gsjava.util.StringUtil;

public class Main {

	public static void main(String[] args) {
		Revision revision = new Revision();
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

			StdIO stdio = new StdIO();

			code = gsapi_set_stdio_with_handle(instanceRef.value, stdio, stdio, stdio, GS_NULL);
			if (code != GS_ERROR_OK) {
				System.err.println("Failed to set stdio with handle (code = " + code + ")");
				gsapi_delete_instance(instanceRef.value);
				instanceRef.value = GS_NULL;
				return;
			}

			String[] gargs = { "gs", "-h" };
			code = gsapi_init_with_args(instanceRef.value, gargs);

		} else {
			System.err.println("Failed to create new instance");
			return;
		}
		gsapi_delete_instance(instanceRef.value);
		instanceRef.value = GS_NULL;
	}
}
