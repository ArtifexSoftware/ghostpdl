package com.artifex.gsviewer.gui;

import java.awt.Dimension;
import java.util.Arrays;
import java.util.Objects;

import javax.swing.JComponent;
import javax.swing.JScrollBar;
import javax.swing.JScrollPane;

import com.artifex.gsviewer.Document;
import com.artifex.gsviewer.Page;

public class ScrollMap {

	private final Document document;
	private final ViewerWindow window;
	private final int gap;
	private int[] scrollMap;

	public ScrollMap(final Document document, final ViewerWindow window, final int gap) {
		this.document = Objects.requireNonNull(document, "document");
		this.scrollMap = null;
		this.window = Objects.requireNonNull(window, "window");
		this.gap = gap;
		genMap(1.0);
	}

	public synchronized void genMap(final double zoom) {
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
			currentScroll += cSize.height;
			currentScroll += gap;
		}
	}

	public synchronized int getScroll(int page) {
		return scrollMap[page - 1];
	}

	public synchronized int getCurrentPage() {
		final JScrollPane scrollPane = window.getViewerScrollPane();
		final JScrollBar vScrollBar = scrollPane.getVerticalScrollBar();
		final int scrollValue = vScrollBar.getValue();

		for (int i = 0; i < scrollMap.length; i++) {
			if (scrollValue > scrollMap[i])
				return i + 1;
		}
		return 0;
	}

	public synchronized void scrollTo(int page) {
		final int scroll = scrollMap[page - 1];
		final JScrollPane scrollPane = window.getViewerScrollPane();
		final JScrollBar vScrollBar = scrollPane.getVerticalScrollBar();
		vScrollBar.setValue(scroll);
	}

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
