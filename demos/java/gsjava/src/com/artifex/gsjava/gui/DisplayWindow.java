package com.artifex.gsjava.gui;

import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Panel;
import java.awt.Point;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.awt.image.ImageObserver;

import com.artifex.gsjava.Page;

public class DisplayWindow {

	private InternalWindow window;

	public DisplayWindow(final Dimension size) {
		window = new InternalWindow(size);
		window.setVisible(true);
	}

	public void renderPage(final Page page) {
		window.renderPanel.page = page;
		window.renderPanel.repaint();
	}

	private class InternalWindow extends Frame implements WindowListener {

		private static final long serialVersionUID = 1L;

		private RenderPanel renderPanel;

		private InternalWindow(final Dimension size) {
			this.renderPanel = new RenderPanel(size);
			add(renderPanel);

			addWindowListener(this);

			pack();
			setTitle("Test Display");
		}

		private class RenderPanel extends Panel {

			private static final long serialVersionUID = 1L;

			private Page page;

			private int bufferWidth, bufferHeight;
			private Image bufferImage;
			private Graphics bufferGraphics;

			private RenderPanel(final Dimension size) {
				setPreferredSize(size);
			}

			private void resetBuffer() {
				bufferWidth = getSize().width;
				bufferHeight = getSize().height;

				if (bufferGraphics != null) {
					bufferGraphics.dispose();
					bufferGraphics = null;
				}

				if (bufferImage != null) {
					bufferImage.flush();
					bufferImage = null;
				}

				System.gc();

				if (bufferWidth > 0 && bufferHeight > 0) {
					bufferImage = createImage(bufferWidth, bufferHeight);
					bufferGraphics = bufferImage.getGraphics();
				}
			}

			private void paintBuffer(Graphics g) {
				final RenderParams params = new RenderParams() {

					@Override
					public Point getLocation() {
						return new Point(0, 0);
					}

					@Override
					public Dimension getViewport() {
						return getSize();
					}

					@Override
					public Graphics getGraphics() {
						return g;
					}

					@Override
					public ImageObserver getObserver() {
						return RenderPanel.this;
					}

				};

				if (page != null)
					page.render(params);
			}

			@Override
			public void paint(Graphics g) {
				if (bufferGraphics != null) {
					paintBuffer(bufferGraphics);
					g.drawImage(bufferImage, 0, 0, this);
				}
			}

			@Override
			public void invalidate() {
				if (bufferWidth != getSize().width || bufferHeight != getSize().height || bufferImage == null ||
						bufferGraphics == null)
					resetBuffer();
				super.invalidate();
			}

			@Override
			public void validate() {
				super.validate();
			}
		}

		@Override
		public void windowOpened(WindowEvent e) {
			// TODO Auto-generated method stub

		}

		@Override
		public void windowClosing(WindowEvent e) {
			System.exit(0);
		}

		@Override
		public void windowClosed(WindowEvent e) {
			// TODO Auto-generated method stub

		}

		@Override
		public void windowIconified(WindowEvent e) {
			// TODO Auto-generated method stub

		}

		@Override
		public void windowDeiconified(WindowEvent e) {
			// TODO Auto-generated method stub

		}

		@Override
		public void windowActivated(WindowEvent e) {
			// TODO Auto-generated method stub

		}

		@Override
		public void windowDeactivated(WindowEvent e) {
			// TODO Auto-generated method stub

		}
	}
}
