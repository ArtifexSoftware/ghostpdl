package com.artifex.gsviewer.gui;

import java.awt.Point;

public interface ViewerGUIListener {

	public void onViewerAdd(ViewerWindow source);

	public void onPageChange(int oldPage, int newPage);

	public void onZoomChange(double oldZoom, double newZoom);

	public void onScrollChange(int newScroll);

	public void onOpenFile();

	public void onCloseFile();

	public void onClosing();

	public void onSettingsOpen();
}
