package com.artifex.gsviewer;

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
import java.util.Objects;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.BytePointer;
import com.artifex.gsjava.util.LongReference;
import com.artifex.gsviewer.ImageUtil.ImageParams;

public class Document implements List<Page> {

	private static final Object slock = new Object();
	private static final DocumentLoader documentLoader = new DocumentLoader();
	private static final int format = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_BIGENDIAN;

	public static void loadDocumentAsync(final File file, final DocAsyncLoadCallback cb)
			throws NullPointerException {
		Objects.requireNonNull(cb, "DocAsyncLoadCallback");
		final Thread t = new Thread(() -> {
			Document doc = null;
			Exception exception = null;
			try {
				doc = new Document(file);
			} catch (FileNotFoundException | IllegalStateException | NullPointerException e) {
				exception = e;
			}
			cb.onDocumentLoad(doc, exception);
		});
		t.setName("Document-Loader-Thread");
		t.setDaemon(true);
		t.start();
	}

	public static void loadDocumentAsync(final String filename, final DocAsyncLoadCallback cb)
			throws NullPointerException {
		loadDocumentAsync(new File(filename), cb);
	}

	@FunctionalInterface
	public static interface DocAsyncLoadCallback {

		public void onDocumentLoad(Document doc, Exception exception);
	}

	private static void initDocInstance(LongReference instanceRef) throws IllegalStateException {
		int code = gsapi_new_instance(instanceRef, GS_NULL);
		if (code != GS_ERROR_OK) {
			gsapi_delete_instance(instanceRef.value);
			throw new IllegalStateException("Failed to set stdio with handle (code = " + code + ")");
		}

		code = gsapi_set_arg_encoding(instanceRef.value, 1);
		if (code != GS_ERROR_OK) {
			gsapi_delete_instance(instanceRef.value);
			throw new IllegalArgumentException("Failed to set arg encoding (code = " + code + ")");
		}

		code = gsapi_set_display_callback(instanceRef.value, documentLoader.reset());
		if (code != GS_ERROR_OK) {
			gsapi_delete_instance(instanceRef.value);
			throw new IllegalStateException("Failed to set display callback code=" + code);
		}
	}

	private static class DocumentLoader extends DisplayCallback {
		private int pageWidth, pageHeight, pageRaster;
		private BytePointer pimage;
		private List<BufferedImage> images;

		private DocumentLoader() {
			reset();
		}

		private DocumentLoader reset() {
			this.pageWidth = 0;
			this.pageHeight = 0;
			this.pageRaster = 0;
			this.pimage = null;
			this.images = new LinkedList<>();
			return this;
		}

		@Override
		public int onDisplaySize(long handle, long device, int width, int height, int raster, int format,
				BytePointer pimage) {
			this.pageWidth = width;
			this.pageHeight = height;
			this.pageRaster = raster;
			this.pimage = pimage;
			return 0;
		}

		@Override
		public int onDisplayPage(long handle, long device, int copies, boolean flush) {
			byte[] data = (byte[]) pimage.toArrayNoConvert();
			images.add(ImageUtil.createImage(data, new ImageParams(pageWidth, pageHeight, pageRaster, BufferedImage.TYPE_3BYTE_BGR)));
			return 0;
		}
	}

	private File file;
	private List<Page> pages;

	public Document(final File file)
			throws FileNotFoundException, IllegalStateException, NullPointerException {
		this.file = Objects.requireNonNull(file, "file");
		if (!file.exists())
			throw new FileNotFoundException(file.getAbsolutePath());

		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_LOW_DPI,
				"-sDEVICE=display", "-dDisplayFormat=" + format, "-f", file.getAbsolutePath() };

		synchronized (slock) {
			LongReference instanceRef = new LongReference();
			initDocInstance(instanceRef);

			int code = gsapi_init_with_args(instanceRef.value, gargs);
			gsapi_exit(instanceRef.value);
			gsapi_delete_instance(instanceRef.value);
			if (code != GS_ERROR_OK) {
				throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
			}

			this.pages = new ArrayList<>(documentLoader.images.size());
			for (BufferedImage img : documentLoader.images) {
				pages.add(new Page(img));
			}
		}
	}

	public Document(final String filename) throws FileNotFoundException, NullPointerException {
		this(new File(Objects.requireNonNull(filename, "filename")));
	}

	public void loadHighRes(int startPage, int endPage) throws IndexOutOfBoundsException, IllegalStateException {
		checkBounds(startPage, endPage);
		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_HIGH_DPI,
				"-sDEVICE=display", "-dFirstPage=" + startPage, "-dLastPage=" + endPage, "-dDisplayFormat=" + format,
				"-dTextAlphaBits=4", "-dGraphicsAlphaBits=4",
				"-f", file.getAbsolutePath() };
		synchronized (slock) {
			LongReference instanceRef = new LongReference();
			initDocInstance(instanceRef);

			int code = gsapi_init_with_args(instanceRef.value, gargs);
			gsapi_exit(instanceRef.value);
			gsapi_delete_instance(instanceRef.value);
			if (code != GS_ERROR_OK) {
				throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
			}

			int ind = startPage - 1;
			for (final BufferedImage img : documentLoader.images) {
				this.pages.get(ind++).setHighRes(img);
			}
		}
	}

	public void loadHighRes(int page) throws IndexOutOfBoundsException, IllegalStateException {
		loadHighRes(page, page);
	}

	public void loadHighResList(final int... pages) throws IndexOutOfBoundsException, IllegalStateException {
		if (pages.length > 0) {
			final StringBuilder builder = new StringBuilder();
			if (pages[0] < 1 || pages[0] > this.pages.size())
				throw new IndexOutOfBoundsException("page=" + pages[0]);
			builder.append(pages[0]);

			for (int i = 1; i < pages.length; i++) {
				if (pages[i] < 1 || pages[i] > this.pages.size())
					throw new IndexOutOfBoundsException("page=" + pages[i]);
				builder.append(',').append(pages[i]);
			}


			final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_HIGH_DPI,
					"-sDEVICE=display", "-sPageList=" + builder.toString(), "-dDisplayFormat=" + format,
					"-dTextAlphaBits=4", "-dGraphicsAlphaBits=4",
					"-f", file.getAbsolutePath() };

			synchronized (slock) {
				LongReference instanceRef = new LongReference();
				initDocInstance(instanceRef);

				int code = gsapi_init_with_args(instanceRef.value, gargs);
				gsapi_exit(instanceRef.value);
				gsapi_delete_instance(instanceRef.value);
				if (code != GS_ERROR_OK) {
					throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
				}

				int ind = 0;
				for (final BufferedImage img : documentLoader.images) {
					this.pages.get(pages[ind] - 1).setHighRes(img);
				}
			}
		}
	}

	public void unloadHighRes(int startPage, int endPage) throws IndexOutOfBoundsException {
		checkBounds(startPage, endPage);
		for (int i = startPage - 1; i < endPage; i++) {
			pages.get(i).unloadHighRes();
		}
	}

	public void unloadHighRes(int page) throws IndexOutOfBoundsException {
		unloadHighRes(page, page);
	}

	private void checkBounds(int start, int end) throws IndexOutOfBoundsException {
		if (start < 1 || start >= pages.size())
			throw new IndexOutOfBoundsException("start=" + start);
		if (end < 1 || end > pages.size())
			throw new IndexOutOfBoundsException("end=" + end);
		if (end < start)
			throw new IndexOutOfBoundsException("end < start");
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