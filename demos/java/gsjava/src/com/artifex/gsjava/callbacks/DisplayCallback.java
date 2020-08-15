package com.artifex.gsjava.callbacks;

import com.artifex.gsjava.util.BytePointer;
import com.artifex.gsjava.util.Reference;

/**
 * Class wrapping a display_callback structure.
 *
 * @author Ethan Vrhel
 *
 */
public abstract class DisplayCallback {

	public volatile int size;
	public volatile int versionMajor;
	public volatile int versionMinor;

	public int onDisplayOpen(long handle, long device) {
		return 0;
	}

	public int onDisplayPreclose(long handle, long device) {
		return 0;
	}

	public int onDisplayClose(long handle, long device) {
		return 0;
	}

	public int onDisplayPresize(long handle, long device, int width,
			int height, int raster, int format) {
		return 0;
	}

	public int onDisplaySize(long handle, long device, int width,
			int height,int raster, int format, BytePointer pimage) {
		return 0;
	}

	public int onDisplaySync(long handle, long device) {
		return 0;
	}

	public int onDisplayPage(long handle, long device, int copies, boolean flush) {
		return 0;
	}

	public int onDisplayUpdate(long handle, long device, int x, int y, int w, int h) {
		return 0;
	}

	/*
	public long onDisplayMemalloc(long handle, long device, long size) {
		return GS_NULL;
	}

	public int onDisplayMemfree(long handle, long device, long mem) {
		return 0;
	}*/

	public int onDisplaySeparation(long handle, long device, int component, byte[] componentName,
			short c, short m, short y, short k) {
		return 0;
	}

	public int onDisplayAdjustBandHeight(long handle, long device, int bandHeight) {
		return 0;
	}

	public int onDisplayRectangleRequest(long handle, long device, Reference<Long> memory, Reference<Integer> ox,
			Reference<Integer> oy, Reference<Integer> raster, Reference<Integer> planeRaster, Reference<Integer> x,
			Reference<Integer> y, Reference<Integer> w, Reference<Integer> h) {
		return 0;
	}
}
