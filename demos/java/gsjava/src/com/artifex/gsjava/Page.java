package com.artifex.gsjava;

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Point;
import java.awt.image.BufferedImage;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.Raster;
import java.io.File;
import java.io.IOException;

import javax.imageio.ImageIO;

import com.artifex.gsjava.gui.RenderParams;

public class Page {

	private final BufferedImage image;

	public Page(final BufferedImage image) {
		this.image = image;
	}

	public Page(final byte[] data, final int width, final int height, final int format) {
		this.image = new BufferedImage(width, height, format);
		this.image.setData(Raster.createRaster(this.image.getSampleModel(),
				new DataBufferByte(data, data.length), new Point()));
		try {
			ImageIO.write(this.image, "PNG", new File("test.png"));
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		//BufferedImage img = null;
		//try {
		//	img = ImageIO.read(new File("image.png"));
		//} catch (IOException e) {
	//		System.out.println("Failed to load image: " + e);
	//		e.printStackTrace();
	//	} finally {
	//		this.image = img;
	//	}
	}

	public void render(final RenderParams params) {
		final Dimension viewport = params.getViewport();
		final Point location = params.getLocation();
		final Graphics g = params.getGraphics();
		final Image img = image.getScaledInstance(viewport.width, viewport.height, Image.SCALE_SMOOTH);
		g.drawImage(img, location.x, location.y, params.getObserver());
	}

	@Override
	public void finalize() {
		image.flush();
	}
}
