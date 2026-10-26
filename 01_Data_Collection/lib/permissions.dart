import 'package:permission_handler/permission_handler.dart';

Future<bool> requestLocalizationPermissions() async {
  Map<Permission, PermissionStatus> statuses = await [
    Permission.storage,
    Permission.location,
    Permission.locationWhenInUse,
  ].request();
  
  print('Permission statuses: $statuses');
  
  return statuses[Permission.location]!.isGranted || statuses[Permission.locationWhenInUse]!.isGranted;
}
