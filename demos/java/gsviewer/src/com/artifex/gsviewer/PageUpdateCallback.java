package com.artifex.gsviewer;

public interface PageUpdateCallback {

	public void onPageUpdate();

	public void onLoadLowRes();
	public void onUnloadLowRes();

	public void onLoadHighRes();
	public void onUnloadHighRes();

	public void onLoadZoomed();
	public void onUnloadZoomed();
}
