<%
    out.println("COUCOU in test.jsp");
    String value1 = "01234567890abcdf";
    String value = value1;
    for (int i=0; i<513; i++) {
      value = value.concat(value1);
    }
    response.setHeader("Test", value);
%>
