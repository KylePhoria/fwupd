
install_data(['fwupd-refresh.service'],
  install_dir: systemdunitdir)
install_data(['fwupd-refresh.timer'],
  install_dir: systemdunitdir)

con2 = configuration_data()
con2.set('bindir', bindir)
configure_file(
  input : '85-fwupd.motd.in',
  output : '85-fwupd',
  configuration : con2,
  install: true,
  install_dir: join_paths (sysconfdir,
                           'update-motd.d')
)
