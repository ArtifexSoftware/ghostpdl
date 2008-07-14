/* Portions Copyright (C) 2001 Artifex Software Inc.
   
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

import java.awt.*;
import java.awt.image.*;
import java.awt.event.*;
import java.io.File;
import javax.swing.*;
import javax.swing.filechooser.*;

/** 
 * Two window nav + zoomed Viewer for PCL and PXL files.
 * 
 * Usage:
 * java Nav ../frs96.pxl
 * 
 * Adds a smaller navigation window coupled to a Gview window.
 * This allows a small portion of a page to be viewed at high resolution
 * in the Gview window with navigation occuring via the Nav window.
 * 
 * Mostly inherits behavior and reflects action on this window and the 
 * Gview zoomed window.
 *
 * @version $Revision: 1643 $
 * @author Stefan Kemper
 */
public class Nav extends Gview  {

    public Nav() {
	pageView = new Gview();
    }

    public void runMain(String[] args) {
	
	// this window is smaller/lower res, count pages now.
	runJob(args, startingRes / zoomWindowRatio, true);

	// zoom window is higher res, don't count pages now.
	pageView.runJob(args, startingRes, false);
    }

    /** main program */
    public static void main( String[] args )
    {
	Nav view = new Nav();

	if (args.length < 1) {  
	    File file = new File("GhostPrinter.pcl");
	    if (file.exists()) {
		// open demo file
		args = new String[1];
		args[0] = new String("GhostPrinter.pcl");
	    }
	}
	System.out.print(usage());

	if (args.length < 1) {
	    // no demo file start with file open
	    view.fileOpen();
	}
        view.runMain(args);
    }

    public void nextPage() {
	pageView.setPage(pageNumber+1);
	super.nextPage();
    }

    public void prevPage() {
	pageView.setPage(pageNumber-1);
        super.prevPage();
    }

    /** low res image is ready, 
     * if we are not getting the next page 
     * start generation of the high res image 
     */
    public void imageIsReady( BufferedImage newImage ) {
	super.imageIsReady(newImage);

	if (!pickle.busy() && !pageView.pickle.busy()) {
	    pageView.pickle.startProduction(pageNumber);
	}
    }

    /** thread counting pages is finished update status display */
    public void pageCountIsReady( int pageCount ) {
	setPageCount( pageCount );
	pageView.setPageCount( pageCount );
    }

    /** moves/drags zoomin box and causes regerenation of a new viewport
     */
    protected void translate(int x, int y) {
	origX -= x;
	origY -= y;
	
        double x1 = origX;
        double y1 = origY;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        double psfx = origX / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = origY / origH * pageView.origH / pageView.origRes * pageView.desiredRes;

	pageView.translateTo( psfx, psfy );
	repaint();
    }

    /** Paint low res image with red zoom box
     *  zoom box uses xor realtime drag.
     */ 
    public void paint( Graphics g )
    {
	int h = (int)(origH * pageView.origRes / pageView.desiredRes);
	int w = (int)(origW * pageView.origRes / pageView.desiredRes);
	if (drag == true) {
	    g.setXORMode(Color.cyan);
	    g.drawRect((int)lastX, (int)lastY, w, h);
	    g.drawRect((int)newX, (int)newY, w, h);
	}
	else {
	    g.setPaintMode();	
	    g.drawImage(currentPage, 0, 0, this);
	    g.setColor(Color.red);  	
	    g.drawRect((int)origX, (int)origY, w, h);
	}
    }

    /** pageView gets regenerated at higher resolution,
     * repaint updates zoomin box.
     */
    protected void zoomIn( int x, int y ) {

        double psfx = x / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = y / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	pageView.origX = pageView.origY = 0;
	pageView.zoomIn((int)psfx,(int)psfy);
	repaint();
    }

    /** pageView gets regenerated at lower resolution,
     * repaint updates zoomin box.
     */
    protected void zoomOut( int x, int y ) {
        double psfx = x / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = y / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	pageView.origX = pageView.origY = 0;
	pageView.zoomOut(0,0);
	repaint();
    }

    /** pageView gets regenerated at requested resolution,
     * repaint updates zoomin box.
     */
    protected void zoomToRes( double res ) {		   
	pageView.zoomToRes(res);  
	repaint();
    }

    /** pageView gets regenerated with the request scalefactor,
     * repaint updates zoomin box. 
     */
    protected void zoomFactor( double factor ) {		   
	pageView.zoomFactor(factor);  
	repaint();
    }

    /**
     * High resolution window.
     * @link aggregationByValue
     */
    private Gview pageView;
}
