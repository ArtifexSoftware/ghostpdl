package com.artifex.gsviewer;

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Point;
import java.awt.color.ColorSpace;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.ComponentColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.PixelInterleavedSampleModel;
import java.awt.image.Raster;
import java.awt.image.SampleModel;
import java.awt.image.WritableRaster;
import java.io.File;
import java.io.IOException;

import javax.imageio.ImageIO;

public class Page {

	private final BufferedImage image;

	public Page(final BufferedImage image) {
		this.image = image;
	}

	public Page(final byte[] data, final int width, final int height, final int raster, final int format) {
		DataBuffer dataBuffer = new DataBufferByte(data, data.length);
		WritableRaster wraster = Raster.createInterleavedRaster(dataBuffer, width, height, raster, 3, new int[] { 0, 1, 2 }, new Point());

	//	this.image = new BufferedImage(width, height, format);
		//this.image.setData(Raster.createRaster(this.image.getSampleModel(),
		//		new DataBufferByte(data, data.length), new Point()));
		ColorModel model = new ComponentColorModel(ColorSpace.getInstance(ColorSpace.CS_sRGB), new int[] { 8, 8, 8 },
				false, false, ColorModel.OPAQUE, DataBuffer.TYPE_BYTE);

		this.image = new BufferedImage(model, wraster, false, null);
		try {
			ImageIO.write(this.image, "PNG", new File("test.png"));
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	@Override
	public void finalize() {
		image.flush();
	}
}