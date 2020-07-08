package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import com.artifex.gsjava.GSAPI.Revision;
import com.artifex.gsjava.util.LongReference;

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
		System.out.println("Instance address: 0x" + Long.toHexString(instanceRef.value));
		System.out.println();

		gsapi_delete_instance(instanceRef.value);
	}
}
