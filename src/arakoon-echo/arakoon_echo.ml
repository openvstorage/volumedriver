open Registry

let echo _ maybe_str = maybe_str

let () = Registry.register "com.openvstorage.volumedriver.echo" echo

(* Local Variables: ** *)
(* mode: Tuareg ** *)
(* compile-command: "ocamlbuild -use-ocamlfind -tag 'package(arakoon_client)' -cflag -thread -lflag -thread arakoon_echo.cmxs" ** *)
(* End: ** *)
