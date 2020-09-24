package com.artifex.gsviewer.gui;

/**
 * This interface provides several methods which are used to listen
 * for events on a <code>ViewerWindow</code>.
 *
 * @author Ethan Vrhel
 *
 */
public interface ViewerGUIListener {

	/**
	 * Called when this listener is added to a <code>ViewerWindow</code>.
	 *
	 * @param source The window added to.
	 */
	public void onViewerAdd(ViewerWindow source);

	/**
	 * Called when the user scrolls to a new page.
	 *
	 * @param oldPage The old page.
	 * @param newPage The new page.
	 */
	public void onPageChange(int oldPage, int newPage);

	/**
	 * Called when the zoom value changes.
	 *
	 * @param oldZoom The old zoom value.
	 * @param newZoom The new zoom value.
	 */
	public void onZoomChange(double oldZoom, double newZoom);

	/**
	 * Called when the user scrolls.
	 *
	 * @param newScroll The scroll value which the user scrolls to.
	 */
	public void onScrollChange(int newScroll);

	/**
	 * Called when a file is opened.
	 */
	public void onOpenFile();

	/**
	 * Called when a file is closed.
	 */
	public void onCloseFile();

	/**
	 * Called when the window is closing.
	 */
	public void onClosing();

	/**
	 * Called when the settings window opens.
	 */
	public void onSettingsOpen();
}
