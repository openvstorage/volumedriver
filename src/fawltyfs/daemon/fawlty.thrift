namespace cpp fawlty_daemon

typedef i64 filesystem_handle_t
typedef string backend_path_t
typedef string frontend_path_t
typedef list<string> fuse_args_t
typedef string filesystem_name_t

exception FawltyException {
    1: string problem;
}

service Fawlty 	{
    i32 ping(),

    filesystem_handle_t
    make_file_system(
    1:backend_path_t backend_path,
    2:frontend_path_t frontend_path,
    3:fuse_args_t fuse_args,
    4:filesystem_name_t filesystem_name) throws (1:FawltyException fe),

    void
    mount(1:filesystem_handle_t filesystem_handle) throws (1:FawltyException fe),

    void
    umount(1:filesystem_handle_t filesystem_handle) throws (1:FawltyException fe),

    oneway void
    stop()
}
