package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.BytePointer;

public class Document implements List<Page> {

	private static final DocumentLoader documentLoader = new DocumentLoader();

	public static Document loadFromFile(final long instance, final String file) throws FileNotFoundException {
		return loadFromFile(instance, new File(file));
	}

	public static Document loadFromFile(final long instance, final File file) throws FileNotFoundException {
		if (!file.exists())
			throw new FileNotFoundException(file.getAbsolutePath());
		int code = gsapi_set_display_callback(instance, documentLoader);
		if (code != GS_ERROR_OK) {
			System.err.println("Failed to set display callback");
			return null;
		}
		final int format = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_BIGENDIAN;
		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER",
				"-I%rom%Resource%/Init/",
				"-dBATCH", "-r72", "-sDEVICE=display",
				"-dDisplayFormat=" + format,
				"-f",
				file.getAbsolutePath() };
		code = gsapi_init_with_args(instance, gargs);
		if (code != GS_ERROR_OK) {
			System.err.println("Failed to gsapi_init_with_args");
			return null;
		}
		return new Document(documentLoader.pages);
	}

	private static class DocumentLoader extends DisplayCallback {
		private int pageWidth, pageHeight, pageRaster;
		private BytePointer pimage;
		private List<Page> pages;

		private DocumentLoader() {
			pages = new LinkedList<>();
		}

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
			pages.add(new Page(data, pageWidth, pageHeight, pageRaster, BufferedImage.TYPE_3BYTE_BGR));
			return 0;
		}
	}

	private final List<Page> pages;

	public Document() {
		this.pages = new ArrayList<>();
	}

	public Document(final List<Page> pages) {
		this.pages = new ArrayList<>(pages);
	}

	@Override
	public int size() {
		return pages.size();
	}

	@Override
	public boolean isEmpty() {
		return pages.isEmpty();
	}

	@Override
	public boolean contains(Object o) {
		return pages.contains(o);
	}

	@Override
	public Iterator<Page> iterator() {
		return pages.iterator();
	}

	@Override
	public Object[] toArray() {
		return pages.toArray();
	}

	@Override
	public <T> T[] toArray(T[] a) {
		return pages.toArray(a);
	}

	@Override
	public boolean add(Page e) {
		return pages.add(e);
	}

	@Override
	public boolean remove(Object o) {
		return pages.remove(o);
	}

	@Override
	public boolean containsAll(Collection<?> c) {
		return pages.containsAll(c);
	}

	@Override
	public boolean addAll(Collection<? extends Page> c) {
		return pages.addAll(c);
	}

	@Override
	public boolean addAll(int index, Collection<? extends Page> c) {
		return pages.addAll(index, c);
	}

	@Override
	public boolean removeAll(Collection<?> c) {
		return pages.removeAll(c);
	}

	@Override
	public boolean retainAll(Collection<?> c) {
		return pages.retainAll(c);
	}

	@Override
	public void clear() {
		pages.clear();
	}

	@Override
	public Page get(int index) {
		return pages.get(index);
	}

	@Override
	public Page set(int index, Page element) {
		return pages.set(index, element);
	}

	@Override
	public void add(int index, Page element) {
		pages.add(index, element);
	}

	@Override
	public Page remove(int index) {
		return pages.remove(index);
	}

	@Override
	public int indexOf(Object o) {
		return pages.indexOf(o);
	}

	@Override
	public int lastIndexOf(Object o) {
		return pages.lastIndexOf(o);
	}

	@Override
	public ListIterator<Page> listIterator() {
		return pages.listIterator();
	}

	@Override
	public ListIterator<Page> listIterator(int index) {
		return pages.listIterator(index);
	}

	@Override
	public List<Page> subList(int fromIndex, int toIndex) {
		return pages.subList(fromIndex, toIndex);
	}

	@Override
	public String toString() {
		return "Document[numPages=" + size() + "]";
	}

	@Override
	public boolean equals(Object o) {
		if (o == this)
			return true;
		if (o instanceof Document) {
			return pages.equals(((Document)o).pages);
		}
		return false;
	}

	@Override
	public int hashCode() {
		return pages.hashCode();
	}
}
