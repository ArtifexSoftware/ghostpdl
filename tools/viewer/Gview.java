import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;


public class Gview extends JFrame implements KeyListener {
    
    static BufferedImage currentPage;
    static int pageNumber = 1;
    static Gpickle page = new Gpickle();
    // constructor
    public Gview() 
    {
	super( "Ghost Pickle Viewer" );
	addKeyListener(this);
    }

    public void keyTyped( KeyEvent e )
    {
    }

    public void keyPressed( KeyEvent e )
    {
	int key = e.getKeyCode();
	// page down NB - increment past last page - BufferedImage
	// will be null and no repaint operation
	if ( key == 34 )
	    pageNumber = pageNumber + 1;
	// page up
	else if ( key == 33 ) {
	    pageNumber = pageNumber - 1;
	    if ( pageNumber < 1 )
		pageNumber = 1;
	}
	page.setPageNumber(pageNumber);
	currentPage = page.getPrinterOutputPage();
	if ( currentPage != null )
	    repaint();
    }

    public void keyReleased( KeyEvent e )
    {
    }

    
    public void paint( Graphics g )
    {
	g.drawImage(currentPage, 0, 0, this);
    }
	    
    
    public static void main( String[] args )
    {
	Gview view = new Gview();
	// NB no error checking.
	page.setJob(args[0]);
	page.setRes(75,75);
	page.setPageNumber(pageNumber);
	currentPage = page.getPrinterOutputPage();
	view.setSize(page.getWidth(), page.getHeight());
	view.show();
    }
}
