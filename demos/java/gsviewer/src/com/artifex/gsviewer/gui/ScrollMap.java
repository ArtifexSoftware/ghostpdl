package com.artifex.gsviewer.gui;

import java.awt.Dimension;
import java.util.Arrays;
import java.util.Objects;

import javax.swing.JComponent;
import javax.swing.JScrollBar;
import javax.swing.JScrollPane;

import com.artifex.gsviewer.Document;

/**
 * A ScrollMap stores information on how far down a scroll bar
 * a certain page is in a viewer.
 *
 * @author Ethan Vrhel
 *
 */
public class ScrollMap {

	private final Document document;
	private final ViewerWindow window;
	private final int gap;
	private int[] scrollMap;

	/**
	 * Creates and generates a new ScrollMap at zoom 1.0.
	 *
	 * @param document The document to generate the map from.
	 * @param window The viewer window to receive scroll panes from.
	 * @param gap The gap in between pages.
	 */
	public ScrollMap(final Document document, final ViewerWindow window, final int gap) {
		this.document = Objects.requireNonNull(document, "document");
		this.scrollMap = null;
		this.window = Objects.requireNonNull(window, "window");
		this.gap = gap;
		genMap(1.0);
	}

	/**
	 * Generates a map at a given zoom amount.
	 *
	 * @param zoom The amount that the document is zoomed in.
	 * @throws IllegalArgumentException If <code>zoom</code> is less than
	 * 0 or greater than 2.
	 */
	public synchronized void genMap(final double zoom) throws IllegalArgumentException {
		if (zoom < 0.0 || zoom > 2.0)
			throw new IllegalArgumentException("0.0 < zoom < 2.0");
		scrollMap = new int[document.size()];
		int currentScroll = 0;
		for (int pageNum = 0; pageNum < document.size(); pageNum++) {
			final JComponent component = window.getPageComponent(pageNum + 1);
			if (component == null)
				throw new NullPointerException("component is null!");
			final Dimension cSize = component.getSize();

			scrollMap[pageNum] = currentScroll;
			currentScroll += cSize.height * zoom;
			currentScroll += gap;
		}
	}

	/**
	 * Returns the amount of scroll on the scroll pane at which page
	 * <code>page</code> is visible.
	 *
	 * @param page The page to get the scroll value for.
	 * @return The respective value.
	 * @throws IndexOutOfBoundsException If <code>page</code> is not a page
	 * in the document.
	 */
	public synchronized int getScroll(int page) throws IndexOutOfBoundsException {
		if (page < 1 || page > scrollMap.length)
			throw new IndexOutOfBoundsException("page = " + page);
		return scrollMap[page - 1];
	}

	/**
	 * Returns the current page scrolled to in the scroll pane.
	 *
	 * @return The scroll value.
	 */
	public synchronized int getCurrentPage() {
		final JScrollPane scrollPane = window.getViewerScrollPane();
		final JScrollBar vScrollBar = scrollPane.getVerticalScrollBar();
		final int scrollValue = vScrollBar.getValue();

		for (int i = 0; i < scrollMap.length; i++) {
			if (scrollValue < scrollMap[i])
				return i; /* Not i + 1! */
		}
		return scrollMap.length;
	}

	/**
	 * Automatically scrolls the scroll pane inside the viewer to the
	 * given page.
	 *
	 * @param page The page to srcoll to.
	 * @throws IndexOutOfBoundsException When <code>page</code> is not in
	 * the document.
	 */
	public synchronized void scrollTo(int page) throws IndexOutOfBoundsException {
		if (page < 1 || page > scrollMap.length)
			throw new IndexOutOfBoundsException("page = " + page);
		final int scroll = scrollMap[page - 1];
		final JScrollPane scrollPane = window.getViewerScrollPane();
		final JScrollBar vScrollBar = scrollPane.getVerticalScrollBar();
		vScrollBar.setValue(scroll);
	}

	/**
	 * Returns a copy of the array backing the scroll map, or
	 * <code>null</code> if none has been generated.
	 *
	 * @return A copy of the scroll map's backing array.
	 */
	public synchronized int[] getMap() {
		if (scrollMap == null)
			return null;
		final int[] copy = new int[this.scrollMap.length];
		System.arraycopy(scrollMap, 0, copy, 0, scrollMap.length);
		return copy;
	}

	@Override
	public String toString() {
		return "ScrollMap[map=" + Arrays.toString(scrollMap) + "]";
	}
}
