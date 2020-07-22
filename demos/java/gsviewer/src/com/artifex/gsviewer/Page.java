package com.artifex.gsviewer;

import java.awt.image.BufferedImage;
import com.artifex.gsviewer.ImageUtil.ImageParams;

public class Page {

	public static final int PAGE_HIGH_DPI = 72;
	public static final int PAGE_LOW_DPI = 10;

	private BufferedImage lowRes;
	private BufferedImage highRes;

	public Page() {
		this(null, null);
	}

	public Page(final BufferedImage lowRes) {
		this(lowRes, null);
	}

	public Page(final BufferedImage lowRes, final BufferedImage highRes) {
		this.lowRes = lowRes;
		this.highRes = highRes;
	}

	public void loadHighRes(final byte[] data, final int width, final int height, final int raster, final int format) {
		setHighRes(ImageUtil.createImage(data, new ImageParams(width, height, raster, format)));
	}

	public void setHighRes(final BufferedImage highRes) {
		unloadHighRes();
		this.highRes = highRes;
	}

	public void unloadHighRes() {
		if (highRes != null) {
			highRes.flush();
			highRes = null;
		}
	}

	public void loadLowRes(final byte[] data, final int width, final int height, final int raster, final int format) {
		setLowRes(ImageUtil.createImage(data, new ImageParams(width, height, raster, format)));
	}

	public void setLowRes(final BufferedImage lowRes) {
		unloadLowRes();
		this.lowRes = lowRes;
	}

	public void unloadLowRes() {
		if (lowRes != null) {
			lowRes.flush();
			lowRes = null;
		}
	}

	public BufferedImage getLowResImage() {
		return lowRes;
	}

	public BufferedImage getHighResImage() {
		return highRes;
	}

	public BufferedImage getDisplayableImage() {
		return highRes == null ? lowRes : highRes;
	}

	@Override
	public String toString() {
		return "Page[lowResLoaded=" + (lowRes != null) + ",highResLoaded=" + (highRes != null) + "]";
	}

	@Override
	public boolean equals(Object o) {
		if (o == this)
			return true;
		if (o instanceof Page) {
			Page p = (Page)o;
			boolean first = lowRes == null ? p.lowRes == null : lowRes.equals(p.lowRes);
			boolean second = highRes == null ? p.highRes == null : highRes.equals(p.highRes);
			return first && second;
		}
		return false;
	}

	@Override
	public int hashCode() {
		return (lowRes == null ? 0 : lowRes.hashCode()) + (highRes == null ? 0 : highRes.hashCode());
	}

	@Override
	public void finalize() {
		unloadLowRes();
		unloadHighRes();
	}
}