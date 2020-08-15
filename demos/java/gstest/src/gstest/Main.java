package gstest;

import static com.artifex.gsjava.GSAPI.*;

import com.artifex.gsjava.GSInstance;
import com.artifex.gsjava.util.NativePointer;
import com.artifex.gsjava.util.Reference;


public class Main {

	public static void main(String[] args) throws Exception {

		GSInstance instance = new GSInstance();
		instance.set_arg_encoding(1);

		String[] gsargs = new String[] {
			"gs",
			"-sDEVICE=pngalpha",
			"-r100"
		};

		instance.init_with_args(gsargs);

		Reference<?> aaRef = new Reference<>();
		instance.get_param_once("TextAlphaBits", aaRef, GS_SPT_INT);
		System.out.println("TextAlphaBits=" + aaRef.getValue());

		instance.set_param("TextAlphaBits", 1, GS_SPT_INT);

		NativePointer value = new NativePointer();
		int bytes = instance.get_param("TextAlphaBits", NativePointer.NULL, GS_SPT_INT);
		value.malloc(bytes);
		instance.get_param("TextAlphaBits", value.getAddress(), GS_SPT_INT);
		int val = NativePointer.intAtNative(value.getAddress(), 0);
		System.out.println(val);
		value.free();

		instance.delete_instance();
	}
}
