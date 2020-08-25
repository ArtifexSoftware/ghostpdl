package com.artifex.gsviewer;

import java.awt.Dimension;
import java.awt.image.BufferedImage;
import java.util.Collection;
import java.util.HashSet;

import com.artifex.gsviewer.ImageUtil.ImageParams;

/**
 * A Page represents an individual page within a Document. It stores a high resolution image,
 * a low resolution preview image, and a zoomed image if the viewer is zoomed.
 *
 * @author Ethan Vrhel
 *
 */
public class Page {

	/**
	 * The high-resolution DPI to use.
	 */
	public static final int PAGE_HIGH_DPI = 72;

	/**
	 * The low-resolution DPI to use.
	 */
	public static final int PAGE_LOW_DPI = 10;

	/**
	 * Converts a dpi value to a Ghostscript-friendly string.
	 *
	 * @param dpi The dpi to convert.
	 * @return The respective string.
	 */
	public static String toDPIString(int dpi) {
		return "[" + dpi + " " + dpi + "]";
	}

	private volatile BufferedImage lowRes; // The low resolution image
	private volatile BufferedImage highRes; // The high resolution image
	private volatile BufferedImage zoomed; // The zoomed image based on the zoom of the viewer

	private Collection<PageUpdateCallback> callbacks; // The page callbacks

	/**
	 * Creates a new page with now low-resolution or high-resolution image.
	 */
	public Page() {
		this(null, null);
	}

	/**
	 * Creates a new page with a low-resolution image.
	 *
	 * @param lowRes The low-resolution image.
	 */
	public Page(final BufferedImage lowRes) {
		this(lowRes, null);
	}

	/**
	 * Creates a new page with a low-resolution image and a high-resolution image.
	 *
	 * @param lowRes The low-resolution image.
	 * @param highRes The high-resolution image.
	 */
	public Page(final BufferedImage lowRes, final BufferedImage highRes) {
		this.lowRes = lowRes;
		this.highRes = highRes;
		this.callbacks = new HashSet<>();
	}

	/**
	 * Loads a high resolution image from data.
	 *
	 * @param data The raw image data.
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param raster The length of one line of the image in bytes.
	 * @param format The image format.
	 */
	public void loadHighRes(final byte[] data, final int width, final int height, final int raster, final int format) {
		setHighRes(ImageUtil.createImage(data, new ImageParams(width, height, raster, format)));
	}

	/**
	 * Sets the high resolution image
	 *
	 * @param highRes An image.
	 */
	public void setHighRes(final BufferedImage highRes) {
		unloadHighRes();
		this.highRes = highRes;
		for (PageUpdateCallback cb : callbacks) {
			cb.onLoadHighRes();
			cb.onPageUpdate();
		}
	}

	/**
	 * Unloads the high-resolution image. If none is loaded, this method
	 * does nothing.
	 */
	public void unloadHighRes() {
		if (highRes != null) {
			highRes.flush();
			highRes = null;
			for (PageUpdateCallback cb : callbacks) {
				cb.onUnloadHighRes();
				cb.onPageUpdate();
			}
		}
	}

	/**
	 * Loads a low resolution image from data.
	 *
	 * @param data The raw image data.
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param raster The length of one line of the image in bytes.
	 * @param format The image format.
	 */
	public void loadLowRes(final byte[] data, final int width, final int height, final int raster, final int format) {
		setLowRes(ImageUtil.createImage(data, new ImageParams(width, height, raster, format)));
	}

	/**
	 * Sets the low resolution image
	 *
	 * @param lowRes An image.
	 */
	public void setLowRes(final BufferedImage lowRes) {
		unloadLowRes();
		this.lowRes = lowRes;
		for (PageUpdateCallback cb : callbacks) {
			cb.onLoadLowRes();
			cb.onPageUpdate();
		}
	}

	/**
	 * Unloads the low-resolution image. If none is loaded, this method
	 * does nothing.
	 */
	public void unloadLowRes() {
		if (lowRes != null) {
			lowRes.flush();
			lowRes = null;
			for (PageUpdateCallback cb : callbacks) {
				cb.onUnloadLowRes();
				cb.onPageUpdate();
			}
		}
	}

	/**
	 * Loads a zoomed image from data.
	 *
	 * @param data The raw image data.
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param raster The length of one line of the image in bytes.
	 * @param format The image format.
	 */
	public void loadZoomed(final byte[] data, final int width, final int height,
			final int raster, final int format) {
		setZoomed(ImageUtil.createImage(data, new ImageParams(width, height, raster, format)));
	}

	/**
	 * Sets the zoomed image
	 *
	 * @param zoomed An image.
	 */
	public void setZoomed(final BufferedImage zoomed) {
		unloadZoomed();
		this.zoomed = zoomed;
		for (PageUpdateCallback cb : callbacks) {
			cb.onLoadZoomed();
			cb.onPageUpdate();
		}
	}

	/**
	 * Unloads the zoomed image. If none is loaded, this method
	 * does nothing.
	 */
	public void unloadZoomed() {
		if (zoomed != null) {
			zoomed.flush();
			zoomed = null;
			for (PageUpdateCallback cb : callbacks) {
				cb.onUnloadZoomed();
				cb.onPageUpdate();
			}
		}
	}

	/**
	 * Unloads all images. If no images are loaded, this method
	 * does nothing.
	 */
	public void unloadAll() {
		unloadLowRes();
		unloadHighRes();
		unloadZoomed();
	}

	/**
	 * Returns the current low-resolution image.
	 *
	 * @return The current low-resolution image, or <code>null</code>
	 * if none is loaded.
	 */
	public BufferedImage getLowResImage() {
		return lowRes;
	}

	/**
	 * Returns the current high-resolution image.
	 *
	 * @return The current high-resolution image, or <code>null</code>
	 * if none is loaded.
	 */
	public BufferedImage getHighResImage() {
		return highRes;
	}

	/**
	 * Returns the current zoomed image.
	 *
	 * @return The current zoomed image, or <code>null</code>
	 * if none is loaded.
	 */
	public BufferedImage getZoomedImage() {
		return zoomed;
	}

	/**
	 * Returns the non-zoomed highest resolution image which can be
	 * displayed.
	 *
	 * @return The highest resolution, non-zoomed image which can be
	 * displayed. If no non-zoomed images are loaded, <code>null</code>
	 * is returned.
	 */
	public BufferedImage getDisplayableImage() {
		return highRes == null ? lowRes : highRes;
	}

	/**
	 * Returns the size of the low-resolution image.
	 *
	 * @return The size of the low-resolution image, or <code>null</code>
	 * if it is not loaded.
	 */
	public Dimension getLowResSize() {
		return lowRes == null ? null : new Dimension(lowRes.getWidth(), lowRes.getHeight());
	}

	/**
	 * Returns the size of the high-resolution image.
	 *
	 * @return The size of the high-resolution image, or <code>null</code>
	 * if it is not loaded.
	 */
	public Dimension getHighResSize() {
		return highRes == null ? null : new Dimension(highRes.getWidth(), highRes.getHeight());
	}

	/**
	 * Returns the size of the zoomed image.
	 *
	 * @return The size of the zoomed image, or <code>null</code>
	 * if it is not loaded.
	 */
	public Dimension getZoomedSize() {
		return zoomed == null ? null : new Dimension(zoomed.getWidth(), zoomed.getHeight());
	}

	/**
	 * Returns the size of the non-zoomed highest resolution image which
	 * can be displayed.
	 *
	 * @return The size of the highest resolution, non-zoomed image which
	 * can be displayed. If no non-zoomed images are loaded, <code>null</code>
	 * is returned.
	 */
	public Dimension getDisplayableSize() {
		return highRes == null ? getLowResSize() : getHighResSize();
	}

	/**
	 * Returns the actual size of the image.
	 *
	 * @return The size of the page, or <code>null</code> if no images are
	 * loaded.
	 */
	public Dimension getSize() {
		Dimension size = getHighResSize();
		if (size != null)
			return size;
		size = getLowResSize();
		if (size == null)
			return new Dimension(0, 0);
		return new Dimension(size.width * PAGE_HIGH_DPI / PAGE_LOW_DPI,
				size.height * PAGE_HIGH_DPI / PAGE_LOW_DPI);
	}

	/**
	 * Adds a <code>PageUpdateCallback</code> to receive callbacks regarding
	 * page change events.
	 *
	 * @param cb The callback to add.
	 */
	public void addCallback(PageUpdateCallback cb) {
		callbacks.add(cb);
		cb.onPageUpdate();
	}

	/**
	 * Removes a callback added through <code>addCallback</code>. If the supplied
	 * callback was not added, this method does nothing.
	 *
	 * @param cb The callback to remove.
	 */
	public void removeCallback(PageUpdateCallback cb) {
		callbacks.remove(cb);
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
		unloadAll();
	}
}
