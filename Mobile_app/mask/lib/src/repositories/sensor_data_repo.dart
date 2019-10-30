import 'package:mask/src/database/models/sensor_model.dart';
import 'package:mask/src/database/access_operations/sensor_data_access.dart';

class SensorDataRepository {
  final sensorDataAccess = SensorDataAccess();

  Future insertSensorData(SensorData sensorData) =>
      sensorDataAccess.createSensorData(sensorData);

  Future deleteAllSensorData() => sensorDataAccess.deleteAllSensorData();

  Future getSensorData(Sensor sensor, {List<DateTime> interval}) =>
      sensorDataAccess.getSensorData(
        sensor,
        interval: interval,
      );
}
