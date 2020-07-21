package com.artifex.gsviewer;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.BytePointer;

public class StandardDisplayCallback extends DisplayCallback {

	private int pageWidth, pageHeight, pageRaster;
	private BytePointer pimage;

	@Override
	public int onDisplaySize(long handle, long device, int width,
			int height, int raster, int format, BytePointer pimage) {
		this.pageWidth = width;
		this.pageHeight = height;
		this.pageRaster = raster;
		this.pimage = pimage;
		System.out.println("width=" + width + ", height=" + height + ", raster=" + raster);
		System.out.println("pimage=" + pimage);
		return 0;
	}

	@Override
	public int onDisplayPage(long handle, long device, int copies, boolean flush) {
		System.out.println("On display page");
		System.out.println("Handle = 0x" + Long.toHexString(handle));
		System.out.println("Device = 0x" + Long.toHexString(device));
		System.out.println("Copies = "+ copies);
		System.out.println("Flush = " + flush);
		byte[] data = (byte[])pimage.toArrayNoConvert();
		return 0;
	}
}
