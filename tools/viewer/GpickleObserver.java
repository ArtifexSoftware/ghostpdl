import java.awt.image.*;

/** Similar to ImageObserver but for BufferedImage
 * @author Stefan Kemper
 * @version $Revision$
 */
public interface GpickleObserver {
    /** newImage contains the completed image */
    void imageIsReady( BufferedImage newImage );
}
