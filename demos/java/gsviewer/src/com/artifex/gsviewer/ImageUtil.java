package com.artifex.gsviewer;

import java.awt.Point;
import java.awt.color.ColorSpace;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.ComponentColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.Raster;
import java.awt.image.WritableRaster;

public class ImageUtil {

	public static BufferedImage createImage(final byte[] data, final ImageParams params) {
		final DataBuffer dataBuffer = new DataBufferByte(data, data.length);
		final WritableRaster wraster = Raster.createInterleavedRaster(dataBuffer, params.width,
				params.height, params.raster, 3, new int[] { 0, 1, 2 }, new Point());
		final ColorModel model = new ComponentColorModel(ColorSpace.getInstance(ColorSpace.CS_sRGB),
				new int[] { 8, 8, 8 }, false, false, ColorModel.OPAQUE, DataBuffer.TYPE_BYTE);

		return new BufferedImage(model, wraster, false, null);
	}

	public static class ImageParams {
		public int width;
		public int height;
		public int raster;
		public int format;

		public ImageParams() {
			this(0, 0, 0, 0);
		}

		public ImageParams(final int width, final int height, final int raster, final int format) {
			this.width = width;
			this.height = height;
			this.raster = raster;
			this.format = format;
		}
	}

	private ImageUtil() { }

}
