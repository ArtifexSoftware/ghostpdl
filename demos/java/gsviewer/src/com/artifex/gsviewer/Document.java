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

/**
 * A Document stores an ordered list of Pages.
 *
 * @author Ethan Vrhel
 *
 */
public class Document implements List<Page> {

	private static final Object slock = new Object();
	private static final DocumentLoader documentLoader = new DocumentLoader();
	private static final int format = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_BIGENDIAN;

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param file The file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param dlb The callback signifying when progress has occurred while loading.
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 */
	public static void loadDocumentAsync(final File file, final DocAsyncLoadCallback cb, final DocLoadProgressCallback dlb)
			throws NullPointerException {
		Objects.requireNonNull(cb, "DocAsyncLoadCallback");
		final Thread t = new Thread(() -> {
			Document doc = null;
			Exception exception = null;
			try {
				doc = new Document(file, dlb);
			} catch (FileNotFoundException | IllegalStateException | NullPointerException e) {
				exception = e;
			}
			cb.onDocumentLoad(doc, exception);
		});
		t.setName("Document-Loader-Thread");
		t.setDaemon(true);
		t.start();
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param file The file to load.
	 * @param cb The callback when loading the document has finished.
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 */
	public static void loadDocumentAsync(final File file, final DocAsyncLoadCallback cb)
			throws NullPointerException {
		loadDocumentAsync(file, cb, null);
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param filename The name of the file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param dlb The callback signifying when progress has occurred while loading.
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 */
	public static void loadDocumentAsync(final String filename, final DocAsyncLoadCallback cb, final DocLoadProgressCallback dlb)
			throws NullPointerException {
		loadDocumentAsync(new File(filename), cb, dlb);
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param filename The name of the file to load.
	 * @param cb The callback when loading the document has finished.
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 */
	public static void loadDocumentAsync(final String filename, final DocAsyncLoadCallback cb)
			throws NullPointerException {
		loadDocumentAsync(filename, cb, null);
	}

	@FunctionalInterface
	public static interface DocAsyncLoadCallback {

		/**
		 * Called when a document has finished loading on a separate
		 * thread.
		 *
		 * @param doc The document which was loaded. <code>null</code> if the document
		 * failed to load.
		 * @param exception An exception that ocurred while loading, <code>null</code>
		 * if no exception was thrown.
		 */
		public void onDocumentLoad(Document doc, Exception exception);
	}

	@FunctionalInterface
	public static interface DocLoadProgressCallback {

		/**
		 * Called when the document has made progress while loading.
		 *
		 * @param progress The amount of progress (from 0 to 100).
		 */
		public void onDocumentProgress(int progress);
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
		private int progress;
		private DocLoadProgressCallback callback;

		private DocumentLoader() {
			reset();
		}

		private DocumentLoader reset() {
			this.pageWidth = 0;
			this.pageHeight = 0;
			this.pageRaster = 0;
			this.pimage = null;
			this.images = new LinkedList<>();
			this.progress = 0;
			this.callback = null;
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

			progress += (100 - progress) / 2;
			if (callback != null)
				callback.onDocumentProgress(progress);

			return 0;
		}
	}

	private File file;
	private List<Page> pages;

	/**
	 * Creates and loads a new document.
	 *
	 * @param file The file to load.
	 * @param loadCallback The callback to indicate when progress has occurred while loading.
	 * @throws FileNotFoundException When <code>file</code> does not exist.
	 * @throws IllegalStateException When Ghostscript fails to intialize or load the document.
	 * @throws NullPointerException When <code>file</code> is <code>null</code>.
	 */
	public Document(final File file, final DocLoadProgressCallback loadCallback)
			throws FileNotFoundException, IllegalStateException, NullPointerException {
		this.file = Objects.requireNonNull(file, "file");
		if (!file.exists())
			throw new FileNotFoundException(file.getAbsolutePath());

		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_LOW_DPI,
				"-sDEVICE=display", "-dDisplayFormat=" + format, "-f", file.getAbsolutePath() };

		synchronized (slock) {
			LongReference instanceRef = new LongReference();
			initDocInstance(instanceRef);

			documentLoader.callback = loadCallback;

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

			if (documentLoader.callback != null)
				documentLoader.callback.onDocumentProgress(100);
		}
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param file The file to load.
	 * @throws FileNotFoundException If the given file does not exist.
	 * @throws NullPointerException If <code>file</code> is <code>null</code>.
	 */
	public Document(final File file)
			throws FileNotFoundException, NullPointerException {
		this(file, null);
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param filename The name of the file to load.
	 * @param loadCallback The callback to indicate when progress has occurred while loading.
	 * @throws FileNotFoundException If the given filename does not exist.
	 * @throws NullPointerException If <code>filename</code> is <code>null</code>.
	 */
	public Document(final String filename, final DocLoadProgressCallback loadCallback)
			throws FileNotFoundException, NullPointerException {
		this(new File(Objects.requireNonNull(filename, "filename")), loadCallback);
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param filename The name of the file to load.
	 * @throws FileNotFoundException If the given filename does not exist.
	 * @throws NullPointerException If <code>filename</code> is <code>null</code>.
	 */
	public Document(final String filename)
			throws FileNotFoundException, NullPointerException {
		this(filename, null);
	}

	/**
	 * Loads the high resolution images in a range of images.
	 *
	 * @param startPage The first page to load.
	 * @param endPage The end page to load.
	 * @throws IndexOutOfBoundsException When <code>firstPage</code> or <code>endPage</code>
	 * are not in the document or <code>endPage</code> is less than <code>firstPage</code>.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 */
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

	/**
	 * Loads the high resolution image of a singular page.
	 *
	 * @param page The page to load.
	 * @throws IndexOutOfBoundsException When <code>page</code> is not in the document.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the
	 * images.
	 */
	public void loadHighRes(int page) throws IndexOutOfBoundsException, IllegalStateException {
		loadHighRes(page, page);
	}

	/**
	 * Loads the high resolution images of a list of pages.
	 *
	 * @param pages The pages to load.
	 * @throws IndexOutOfBoundsException When any page is not a page in the document.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the
	 * images.
	 */
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

	/**
	 * Unloads the high resolution images in a range of pages.
	 *
	 * @param startPage The start page to unload the high resolution image from.
	 * @param endPage The end page to unload the high resolution image from.
	 * @throws IndexOutOfBoundsException When <code>startPage</code> and <code>endPage</code>
	 * are outside document or <code>endPage</code> is less than <code>startPage</code>.
	 */
	public void unloadHighRes(int startPage, int endPage) throws IndexOutOfBoundsException {
		checkBounds(startPage, endPage);
		for (int i = startPage - 1; i < endPage; i++) {
			pages.get(i).unloadHighRes();
		}
	}

	/**
	 * Unloads a high resolution image at the given page.
	 *
	 * @param page The page to unload the high resolution image.
	 * @throws IndexOutOfBoundsException When page is not a page in the document.
	 */
	public void unloadHighRes(int page) throws IndexOutOfBoundsException {
		unloadHighRes(page, page);
	}

	/**
	 * Unloads the document's resources. The document will be unusable after this
	 * call.
	 */
	public void unload() {
		for (Page p : pages) {
			p.unloadLowRes();
			p.unloadHighRes();
		}
		pages.clear();
	}

	/**
	 * Returns the name of the document loaded.
	 *
	 * @return The name.
	 */
	public String getName() {
		return file.getName();
	}

	private void checkBounds(int start, int end) throws IndexOutOfBoundsException {
		if (start < 1 || start > pages.size())
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

	@Override
	public void finalize() {
		unload();
	}
}