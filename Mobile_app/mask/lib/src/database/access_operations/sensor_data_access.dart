import 'dart:async';
import 'package:mask/src/database/database.dart';
import 'package:mask/src/database/models/sensor_data_model.dart';
import 'package:mask/src/widgets/graph/time_series.dart';

class SensorDataAccess {
  final dbProvider = DatabaseProvider.dbProvider;

  Future<int> createSensorData(SensorData sensorData) async {
    final db = await dbProvider.database;
//    print(sensorData.toDatabaseJson().toString());
    var result = db.insert(sensorDataTABLE, sensorData.toDatabaseJson());
    return result;
  }

  Future<List<SensorData>> getSensorData(
      {List<Sensor> sensors, List<DateTime> interval}) async {
    final db = await dbProvider.database;
    List<Map<String, dynamic>> result;
    String query = 'SELECT * FROM $sensorDataTABLE ';

    if (sensors != null) {
      query += ' WHERE sensorName = ';
      for (var i = 0; i < sensors.length; i++) {
        query += '\'${sensors[i].toString()}\'';
        if (i < sensors.length - 1) {
          query += ' OR sensorName = ';
        }
      }
    }
    if (interval != null) {
      if (sensors != null) {
        query += ' AND ';
      } else {
        query += ' WHERE ';
      }
      query += ' timeStamp > ${interval[0].millisecondsSinceEpoch} '
          'AND timeStamp < ${interval[1].millisecondsSinceEpoch}';
    }

    print('query = ' + query);
    result = await db.rawQuery(query);

    List<SensorData> sensorData =
        result.map((item) => SensorData.fromDatabaseJson(item)).toList();
    return sensorData;
  }

  Future<int> deleteAllSensorData() async {
    final db = await dbProvider.database;
    var result = await db.rawDelete('DELETE FROM $sensorDataTABLE');
    return result;
  }
}
