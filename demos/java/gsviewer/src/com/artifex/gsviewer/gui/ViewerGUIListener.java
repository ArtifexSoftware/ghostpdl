package com.artifex.gsviewer.gui;

import java.awt.Point;

public interface ViewerGUIListener {

	public void onPageChange(ViewerWindow source, int oldPage, int newPage);

	public void onZoomChange(ViewerWindow source, double oldZoom, double newZoom);

	public void onScrollChange(ViewerWindow source, Point oldScroll, Point newScroll);

	public void onOpenFile(ViewerWindow source);

	public void onCloseFile(ViewerWindow source);

	public void onClosing(ViewerWindow source);

	public void onSettingsOpen(ViewerWindow source);
}
