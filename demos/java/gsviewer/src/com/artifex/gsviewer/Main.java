package com.artifex.gsviewer;

import java.io.IOException;

import javax.swing.SwingUtilities;
import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;

import com.artifex.gsviewer.gui.ViewerWindow;
import com.artifex.gsjava.util.BytePointer;

public class Main {

	public static void main(String[] args) {
		Runtime.getRuntime().addShutdownHook(new Thread(new Shutdown()));

		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (ClassNotFoundException | InstantiationException | IllegalAccessException
				| UnsupportedLookAndFeelException e) {
			System.err.println("Failed to set Look and Feel: " + e);
		}

		ViewerWindow win = new ViewerWindow(new ViewerController());
		SwingUtilities.invokeLater(() -> {
			win.setVisible(true);
		});
	}

	private static class Shutdown implements Runnable {

		@Override
		public void run() {
			try {
				Settings.SETTINGS.save();
			} catch (IOException e) {
				System.err.println("Failed to write settings file");
				e.printStackTrace();
			}
		}

	}
}
