#!/usr/bin/env python2

# -*- coding: utf-8 -*-

import sys
import time

import rosgraph
import rosnode
import rospy
from opcua import Server, ua, uamethod

import ros_services
import ros_topics


# Returns the hierachy as one string from the first remaining part on.
def nextname(hierachy, index_of_last_processed):
    try:
        output = ""
        counter = index_of_last_processed + 1
        while counter < len(hierachy):
            output += hierachy[counter]
            counter += 1
        return output
    except Exception as e:
        rospy.logerr("Error encountered ", e)


def own_rosnode_cleanup():
    pinged, unpinged = rosnode.rosnode_ping_all()
    if unpinged:
        master = rosgraph.Master(rosnode.ID)
        # noinspection PyTypeChecker
        rosnode.cleanup_master_blacklist(master, unpinged)


class ROSServer:
    def __init__(self):
        self.namespace_ros = rospy.get_param("/rosopcua/namespace")
        self.topicsDict = {}
        self.servicesDict = {}
        self.actionsDict = {}
        rospy.init_node("rosopcua")
        self.server = Server()
        self.server.set_endpoint("opc.tcp://0.0.0.0:4841/")
        self.server.set_server_name("ROS ua Server")
        self.server.start()
        # setup our own namespaces, this is expected
        uri_topics = "http://ros.org/topics"
        # two different namespaces to make getting the correct node easier for get_node (otherwise had object for service and topic with same name
        uri_services = "http://ros.org/services"
        uri_actions = "http://ros.org/actions"
        uri = "http://ros.org/variables"
        idx_variables= self.server.register_namespace(uri)
        idx_topics = self.server.register_namespace(uri_topics)
        idx_services = self.server.register_namespace(uri_services)
        idx_actions = self.server.register_namespace(uri_actions)
        # get Objects node, this is where we should put our custom stuff
        objects = self.server.get_objects_node()
        # one object per type we are watching
        topics_object = objects.add_object(idx_topics, "ROS-Topics")
        services_object = objects.add_object(idx_services, "ROS-Services")
        actions_object = objects.add_object(idx_actions, "ROS_Actions")
        variables_object = objects.add_object(idx_variables, "CustomObject")
        
        #variables
        self.my_variable = variables_object.add_variable(idx_variables, "MyIntegerVariable", 42)
        self.my_variable.set_writable()

        self.my_float_variable = variables_object.add_variable(idx_variables, "MyFloatVariable", 3.14)
        self.my_float_variable.set_writable()

        self.my_bool_variable = variables_object.add_variable(idx_variables, "MyBooleanVariable", True)
        self.my_bool_variable.set_writable()

        self.my_var_string = variables_object.add_variable(idx_variables, "MyStringVariable", "Hello, OPC UA!")
        self.my_var_string.set_writable()  # Make the variable writable

        self.my_static = variables_object.add_variable(idx_variables, "Estatica", 0.0)  # Valor inicial es 0.0
        self.my_static.set_writable()

        variables_object.add_method(idx_variables, "UpdateMyVariable", self.update_variable, [ua.VariantType.Float], [ua.VariantType.StatusCode])
        variables_object.add_method(idx_variables, "GetMyVariable", self.get_my_variable, [], [ua.VariantType.Int32])

        # 1. Crear un nuevo nodo llamado "MyCustomNode" bajo el nodo principal "Objects"
        self.custom_node = objects.add_object(idx_variables, "MyCustomNode")

        # 2. Agregar una variable al nuevo nodo "MyCustomNode"
        self.custom_array_variable = self.custom_node.add_variable(
            idx_variables, "MyArrayVariable", [1342, 232, 13, 44, 85, 662]
        )
        self.custom_array_variable.set_writable()  # Hacer la variable modificable

        rospy.loginfo("Nodo MyCustomNode con MyArrayVariable ha sido creado en el servidor OPC UA.")

        # Nodo "Desglose" que contiene las variables individuales del array
        self.desglose_node = variables_object.add_object(idx_variables, "Desglose")
        self.desglose_variables = []
        array_values = self.custom_array_variable.get_value()

        # Crear variables en el nodo desglose
        for i, value in enumerate(array_values):
            var = self.desglose_node.add_variable(
                idx_variables, "ArrayElement_{}".format(i), value
            )
            var.set_writable()  # Hacer cada variable de desglose modificable
            self.desglose_variables.append(var)

        rospy.loginfo("Nodo Desglose con variables individuales ha sido creado en el servidor OPC UA.")

        rospy.loginfo("Servidor OPC UA esta corriendo con las variables personalizadas")

        while not rospy.is_shutdown():

            # Obtener valores actualizados del array y asignarlos a las variables de desglose
            array_values = self.custom_array_variable.get_value()
            for i, value in enumerate(array_values):
                if i < len(self.desglose_variables):
                    self.desglose_variables[i].set_value(value)


            # Update variable values dynamically (optional)
            self.my_variable.set_value(self.my_variable.get_value() + 1)
            self.my_float_variable.set_value(self.my_float_variable.get_value() + 0.1)
            self.my_var_string.set_value("Current Value: " + str(self.my_variable.get_value()))

            # ros_topics starts a lot of publisher/subscribers, might slow everything down quite a bit.
            ros_services.refresh_services(self.namespace_ros, self, self.servicesDict, idx_services, services_object)
            ros_topics.refresh_topics_and_actions(self.namespace_ros, self, self.topicsDict, self.actionsDict,
                                                  idx_topics, idx_actions, topics_object, actions_object)
            # Don't clog cpu
            time.sleep(1)
        self.server.stop()
        quit()

    @uamethod
    def update_variable(self, parent, new_value):
        """
        Metodo que sera llamado desde el cliente OPC UA para actualizar el valor de la variable.
        """
        try:
            # Actualizar el valor de la variable
            self.my_static.set_value(new_value)
            rospy.loginfo("Variable actualizada a: {}".format(new_value))
            return ua.StatusCode(ua.StatusCodes.Good)
        except Exception as e:
            rospy.logerr("Error actualizando la variable: {}".format(e))
            return ua.StatusCode(ua.StatusCodes.BadInternalError)

    @uamethod
    def get_my_variable(self, parent):
        """
        Metodo que sera llamado desde el cliente OPC UA para obtener el valor de my_variable.
        """
        try:
            current_value = self.my_variable.get_value()
            rospy.loginfo("Valor actual de my_variable: {}".format(current_value))
            return current_value
        except Exception as e:
            rospy.logerr("Error al obtener el valor de la variable: {}".format(e))
            return ua.StatusCode(ua.StatusCodes.BadInternalError)

    def run(self):
        """
        Metodo para ejecutar el servidor en un loop
        """
        try:
            rospy.loginfo("Servidor OPC UA ejecutandose.")
            while not rospy.is_shutdown():
                time.sleep(1)
        finally:
            self.server.stop()
            rospy.loginfo("Servidor OPC UA detenido.")




    def find_service_node_with_same_name(self, name, idx):
        rospy.logdebug("Reached ServiceCheck for name " + name)
        for service in self.servicesDict:
            rospy.logdebug("Found name: " + str(self.servicesDict[service].parent.nodeid.Identifier))
            if self.servicesDict[service].parent.nodeid.Identifier == name:
                rospy.logdebug("Found match for name: " + name)
                return self.servicesDict[service].parent
        return None

    def find_topics_node_with_same_name(self, name, idx):
        rospy.logdebug("Reached TopicCheck for name " + name)
        for topic in self.topicsDict:
            rospy.logdebug("Found name: " + str(self.topicsDict[topic].parent.nodeid.Identifier))
            if self.topicsDict[topic].parent.nodeid.Identifier == name:
                rospy.logdebug("Found match for name: " + name)
                return self.topicsDict[topic].parent
        return None

    def find_action_node_with_same_name(self, name, idx):
        rospy.logdebug("Reached ActionCheck for name " + name)
        for topic in self.actionsDict:
            rospy.logdebug("Found name: " + str(self.actionsDict[topic].parent.nodeid.Identifier))
            if self.actionsDict[topic].parent.nodeid.Identifier == name:
                rospy.logdebug("Found match for name: " + name)
                return self.actionsDict[topic].parent
        return None


def main(args):
    rosserver = ROSServer()
    rosserver.run()


if __name__ == "__main__":
    main(sys.argv)

    rosserver = ROSServer()
    rosserver.run()


if __name__ == "__main__":
    main(sys.argv)
