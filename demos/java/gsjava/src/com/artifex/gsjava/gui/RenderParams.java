package com.artifex.gsjava.gui;

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Point;
import java.awt.image.ImageObserver;

public interface RenderParams {

	public Point getLocation();
	public Dimension getViewport();
	public Graphics getGraphics();
	public ImageObserver getObserver();
}
