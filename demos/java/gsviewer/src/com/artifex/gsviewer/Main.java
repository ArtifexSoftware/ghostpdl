package com.artifex.gsviewer;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import javax.imageio.ImageIO;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;

import com.artifex.gsviewer.gui.ViewerWindow;

public class Main {

	public static void main(String[] args) throws FileNotFoundException {
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (ClassNotFoundException | InstantiationException | IllegalAccessException
				| UnsupportedLookAndFeelException e1) {
			e1.printStackTrace();
		}

		ViewerWindow win = new ViewerWindow(new ViewerGUIListenerImpl());
		SwingUtilities.invokeLater(() -> {
			win.setVisible(true);
		});


		//Document doc = new Document("010104_momentum_concepts.pdf");
		//doc.loadHighRes(1, 3);
		//doc.unloadHighRes(2);
		//doc.loadHighResList(1, 2, 3);

		//int pageNum = 1;
		//for (Page page : doc) {
		//	try {
		//		ImageIO.write(page.getDisplayableImage(), "PNG", new File("page-" + pageNum + ".png"));
		//	} catch (IOException e) {
		//		System.err.println("Failed to write page " + pageNum);
		//	}
		//	pageNum++;
		//}

	}
}
