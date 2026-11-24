fn main() {
    println!("cargo:rustc-link-arg-cdylib=/DELAYLOAD:palmwave-probe-delayed.dll");
    println!("cargo:rustc-link-lib=delayimp");
}
