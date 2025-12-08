<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0" xmlns="http://www.w3.org/1999/xhtml">

<!--

XML stylesheet for onebigfile.xml
Copyright (c) 2010-2022 Arm Limited or its affiliates. All rights reserved.
This document is Non-Confidential.

-->

  <xsl:output doctype-system="xml_dtd.txt"
    method="xml" encoding="utf-8"/>

  <xsl:preserve-space elements="line"/>

  <xsl:template match="/allinstrs">
    <chapter id="{@id}" xreflabel="{@xreflabel}">
      <xsl:apply-templates/>
    </chapter>
  </xsl:template>

  <xsl:template match="title">
    <title><xsl:value-of select="."/></title>
  </xsl:template>

  <xsl:template match="para">
    <para><xsl:value-of select="."/></para>
  </xsl:template>

  <xsl:template match="file">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="sect1">
    <sect1 id="{@id}">
      <xsl:apply-templates/>      
    </sect1>
  </xsl:template>

  <xsl:template match="alphaindex">
    <sect1>
      <xsl:apply-templates/>      
    </sect1>
  </xsl:template>

  <xsl:template match="alphaindex/toptitle"></xsl:template>

  <xsl:template match="alphaindex/iforms">
    <title><xsl:value-of select="@title"/></title>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="alphaindex/iforms/iform">
    <!-- we'll create an xref of type TitleShort to each iform page -->
    <para>
      <xsl:choose>
        <xsl:when test="contains(@iformfile, '#')">
          <xref role="TitleShort" linkend="{@id}"/>:          
        </xsl:when>
        <xsl:otherwise>
          <!-- hard instruction list -->
          <xref role="TitleShort" linkend="{@id}_iformsect"/>:          
        </xsl:otherwise>
      </xsl:choose>
      <xsl:value-of select="."/>
    </para>
  </xsl:template>

  <xsl:template match="encodingindex">
    <title>Index by encodings</title>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="encodingindex/maintable">
    <!-- wrap it in a nblock so we can use the whole width -->
    <para>
    <blockquote>
      <table id="encoding-maintable" colsep="1" frame="all" rowsep="1" pgwide="1" tabstyle="maintable">
        <tabletitle>Instruction class encodings</tabletitle>
        <tgroup cols="{@howmanybits + 1}" align="center">
          <xsl:for-each select="col">
            <colspec colname="{@colno}" colwidth="{@printwidth}"/>
          </xsl:for-each>
          <xsl:apply-templates/>
        </tgroup>
      </table>
    </blockquote>
    </para>
  </xsl:template>

  <xsl:template match="tablebody/maintablesect">
    <row>
      <entry namest="1">
        <xsl:attribute name="nameend">
          <xsl:value-of select="../../@howmanybits+1"/>
        </xsl:attribute>
        <xsl:value-of select="@sect"/>
      </entry>
    </row>
  </xsl:template>

  <xsl:template match="tableheader">
    <thead>
      <xsl:apply-templates />
    </thead>
  </xsl:template>

  <xsl:template match="thead">
    <thead>
      <xsl:apply-templates />
    </thead>
  </xsl:template>

  <xsl:template match="tr">
    <row>
      <xsl:apply-templates />
    </row>
  </xsl:template>

  <xsl:template match="tr/th">
    <entry>
      <xsl:if test="@colspan > 1">
        <xsl:attribute name="namest">
          <xsl:value-of select="@colno"/>
        </xsl:attribute>
        <xsl:attribute name="nameend">
          <xsl:value-of select="@colno + @colspan - 1"/>
        </xsl:attribute>
      </xsl:if>
      <xsl:if test="@rowspan > 1">
        <xsl:attribute name="morerows">
          <xsl:value-of select="@rowspan - 1"/>
        </xsl:attribute>
        <xsl:attribute name="valign">middle</xsl:attribute>
      </xsl:if>
      <xsl:if test="@class = 'boxright'">
        <xsl:attribute name="colsep">1</xsl:attribute>
      </xsl:if>
      <xsl:if test="../@class = 'header1' and not(@rowspan)">
        <xsl:attribute name="rowsep">0</xsl:attribute>
      </xsl:if>
      <xsl:value-of select="." />
    </entry>
  </xsl:template>

  <xsl:template match="tablebody">
    <tbody>
      <xsl:apply-templates />
    </tbody>
  </xsl:template>

  <xsl:template match="tbody">
    <tbody>
      <xsl:apply-templates />
    </tbody>
  </xsl:template>

  <xsl:template match="tr/td">
    <entry>
      <xsl:if test="@colspan > 1">
        <xsl:attribute name="namest">
          <xsl:value-of select="@colno"/>
        </xsl:attribute>
        <xsl:attribute name="nameend">
          <xsl:value-of select="@colno + @colspan - 1"/>
        </xsl:attribute>
      </xsl:if>
      <xsl:if test="@rowspan > 1">
        <xsl:attribute name="morerows">
          <xsl:value-of select="@rowspan - 1"/>
        </xsl:attribute>
      </xsl:if>
      <xsl:if test="@class = 'boxright'">
        <xsl:attribute name="colsep">1</xsl:attribute>
      </xsl:if>
      <xsl:if test="../@class = 'header2' and not(@class = 'boxright')">
        <xsl:attribute name="colsep">0</xsl:attribute>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@iclass">
          <xref role="TitleShort" linkend="{@iclass}"/>:
        </xsl:when>
        <xsl:when test="@class = 'iclassname'">
          <!-- encodingindex maintable classname -->
          <xsl:attribute name="align">left</xsl:attribute>
          <xsl:if test=". != 'UNALLOCATED'">
            <xref role="TitleShort" linkend="{../@iclass}"/>
          </xsl:if>
        </xsl:when>
        <xsl:when test="@iformid">
          <xsl:attribute name="align">left</xsl:attribute>
          <xref role="TitleShort" linkend="{@iformid}_iformsect"/>            
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates />
        </xsl:otherwise>
      </xsl:choose>
    </entry>
  </xsl:template>

  <xsl:template match="encodingindex/iclass_sect">
    <sect2 id="{@id}" xreflabel="{@id}">
      <title>
        <xsl:value-of select="@title"/>
      </title>
      <xsl:apply-templates />
    </sect2>
  </xsl:template>

  <xsl:template match="iclass_sect/instructiontable">
    <table id="{@iclass}_table" colsep="1" frame="all" rowsep="1" pgwide="1" tabstyle="instructiontable">
      <tgroup cols="{@cols}" align="center">
        <xsl:for-each select="col">
          <colspec colname="{position()}" colwidth="{@printwidth}" />
        </xsl:for-each>
        <xsl:apply-templates />
      </tgroup>
    </table>
  </xsl:template>

  <xsl:template match="iclass_sect/instructiontable/col">
  </xsl:template>

  <xsl:template match="instructionsection">
    <sect2 id="{@id}_iformsect" xreflabel="{@id}_iformsect" role="break">
      <xsl:apply-templates/>
    </sect2>
  </xsl:template>

  <xsl:template match="instructionsection/heading">
    <title><xsl:value-of select="."/></title>
  </xsl:template>

  <xsl:template match="instructionsection/desc">
    <para>
      <xsl:apply-templates/>.
    </para>
  </xsl:template>

  <xsl:template match="instructionsection/desc/brief">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="instructionsection/desc/longer">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="instructionsection/desc/alg">
    : <instruction><xsl:value-of select="."/></instruction>
  </xsl:template>

  <xsl:template match="instructionsection/classes">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="instructionsection/classes/classesintro">
    <xsl:if test="@count > 1">
      <para><xsl:apply-templates /></para>
    </xsl:if>
  </xsl:template>

  <xsl:template match="iclassintro/txt">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="iclassintro/a">
    <xref linkend="{../../@id}" role="TitleShort">
      <!-- <xsl:attribute name="text"><xsl:value-of select="."/></xsl:attribute> -->
    </xref>
  </xsl:template>

  <xsl:template match="instructionsection/classes/iclass">
    <xsl:choose>
      <xsl:when test="@oneof > 1">
        <sect3 id="{../../@id}_{@id}">
          <title><xsl:value-of select="@name"/></title>
          <xsl:apply-templates />
        </sect3>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match ="iclass/iclassintro">
    <xsl:if test="@count > 1 or ../@oneof = 1">
      <para><xsl:apply-templates /></para>
    </xsl:if>
  </xsl:template>

  <xsl:template match ="iclass/encoding">
    <xsl:choose>
      <xsl:when test="@oneofinclass > 1 and ../@oneof > 1">
        <sect4 id="{@name}">
          <title><xsl:value-of select="@label"/></title>
          <xsl:apply-templates />
        </sect4>
      </xsl:when>
      <xsl:when test="@oneofinclass > 1">
        <sect3 id="{@name}">
          <title><xsl:value-of select="@label"/></title>
          <xsl:apply-templates />
        </sect3>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="encoding/asmtemplate">
    <para><instruction><xsl:value-of select="."/></instruction></para>
  </xsl:template>

  <xsl:template match="encoding/diagramtemplate">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="regdiagram">
    <table colsep="0" frame="bottom" rowsep="1" tabstyle="ENCODING" pgwide="1">
      <tgroup cols="32" align="center">
        <colspec colname="1" colnum="1" colwidth="1*" />
        <colspec colname="2" colnum="2" colwidth="1*" />
        <colspec colname="3" colnum="3" colwidth="1*" />
        <colspec colname="4" colnum="4" colwidth="1*" />
        <colspec colname="5" colnum="5" colwidth="1*" />
        <colspec colname="6" colnum="6" colwidth="1*" />
        <colspec colname="7" colnum="7" colwidth="1*" />
        <colspec colname="8" colnum="8" colwidth="1*" />
        <colspec colname="9" colnum="9" colwidth="1*" />
        <colspec colname="10" colnum="10" colwidth="1*" />
        <colspec colname="11" colnum="11" colwidth="1*" />
        <colspec colname="12" colnum="12" colwidth="1*" />
        <colspec colname="13" colnum="13" colwidth="1*" />
        <colspec colname="14" colnum="14" colwidth="1*" />
        <colspec colname="15" colnum="15" colwidth="1*" />
        <colspec colname="16" colnum="16" colwidth="1*" />
        <colspec colname="17" colnum="17" colwidth="1*" />
        <colspec colname="18" colnum="18" colwidth="1*" />
        <colspec colname="19" colnum="19" colwidth="1*" />
        <colspec colname="20" colnum="20" colwidth="1*" />
        <colspec colname="21" colnum="21" colwidth="1*" />
        <colspec colname="22" colnum="22" colwidth="1*" />
        <colspec colname="23" colnum="23" colwidth="1*" />
        <colspec colname="24" colnum="24" colwidth="1*" />
        <colspec colname="25" colnum="25" colwidth="1*" />
        <colspec colname="26" colnum="26" colwidth="1*" />
        <colspec colname="27" colnum="27" colwidth="1*" />
        <colspec colname="28" colnum="28" colwidth="1*" />
        <colspec colname="29" colnum="29" colwidth="1*" />
        <colspec colname="30" colnum="30" colwidth="1*" />
        <colspec colname="31" colnum="31" colwidth="1*" />
        <colspec colname="32" colnum="32" colwidth="1*" />
        <thead>
          <row>
              <entry>31</entry>
              <entry>30</entry>
              <entry>29</entry>
              <entry>28</entry>
              <entry>27</entry>
              <entry>26</entry>
              <entry>25</entry>
              <entry>24</entry>
              <entry>23</entry>
              <entry>22</entry>
              <entry>21</entry>
              <entry>20</entry>
              <entry>19</entry>
              <entry>18</entry>
              <entry>17</entry>
              <entry>16</entry>
              <entry>15</entry>
              <entry>14</entry>
              <entry>13</entry>
              <entry>12</entry>
              <entry>11</entry>
              <entry>10</entry>
              <entry>9</entry>
              <entry>8</entry>
              <entry>7</entry>
              <entry>6</entry>
              <entry>5</entry>
              <entry>4</entry>
              <entry>3</entry>
              <entry>2</entry>
              <entry>1</entry>
              <entry>0</entry>
            </row>
          </thead>
          <tbody>
            <row>
              <xsl:apply-templates/>
            </row>
          </tbody>
        </tgroup>
      </table>
  </xsl:template>

  <xsl:template match="regdiagram/box">
    <xsl:for-each select="c">
      <entry>
        <xsl:choose>
          <xsl:when test="@colspan">
            <!-- <xsl:attribute name="hibit"><xsl:value-of select="@hibit"/></xsl:attribute> -->
            <xsl:attribute name="nameend"><xsl:value-of select="30-../@hibit+@colspan+position()"/></xsl:attribute>
            <xsl:attribute name="namest"><xsl:value-of select="31-../@hibit+position()"/></xsl:attribute>
          </xsl:when>
          <xsl:otherwise>
            <xsl:attribute name="colname"><xsl:value-of select="31-../@hibit+position()"/></xsl:attribute>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="position() = last()">
          <xsl:attribute name="colsep">1</xsl:attribute>            
        </xsl:if>
        <xsl:apply-templates/>
      </entry>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="aliasmnem">
    <sect3 id="{@id}" xreflabel="{@id}">
      <title><xsl:value-of select="@mnemonic"/></title>
      <xsl:apply-templates/>
    </sect3>
  </xsl:template>

  <xsl:template match="aliasmnem/alias">
    <para role="alias-explanation">
      <xsl:if test="@enctag">
        <emphasis><xsl:value-of select="@enctag"/> encodings</emphasis>:
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@conditions">
          When <xsl:value-of select="@conditions"/>, the
        </xsl:when>
        <xsl:otherwise>
          The
        </xsl:otherwise>
      </xsl:choose>
      instruction written:
    </para>
    <para><instruction><xsl:value-of select="@assembler_prototype"/></instruction></para>
    <para role="alias-explanation">is equivalent to:</para>
    <para><instruction><xsl:value-of select="@equivalent_to"/>.</instruction></para>
  </xsl:template>

  <xsl:template match="explanations">
    <xsl:choose>
      <xsl:when test="./explanation_intro/@headingsabove > 1">
        <sect3>
          <title><xsl:value-of select="./explanation_intro/."/></title>
          <para>
            <paramlist language="Assembler">
              <xsl:apply-templates/>
            </paramlist>
          </para>
        </sect3>
      </xsl:when>
      <xsl:otherwise>
        <para><xsl:value-of select="./explanation_intro/."/></para>        
        <para>
          <paramlist language="Assembler">
            <xsl:apply-templates/>
          </paramlist>
        </para>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="explanations/explanation_intro">
  </xsl:template>

  <xsl:template match="explanation">
    <paramlistentry>
      <xsl:apply-templates/>    
   </paramlistentry>
  </xsl:template>

  <xsl:template match="explanation/symbol">
    <parameter><xsl:value-of select="."/></parameter>
  </xsl:template>

  <xsl:template match="explanation/account">
    <listitem>
      <para>
        <xsl:apply-templates/>
      </para>
    </listitem>
  </xsl:template>

  <xsl:template match="explanation/account/intro">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="explanation/account/bullets">
    <itemizedlist role="compressed">
      <xsl:apply-templates/>
    </itemizedlist>
  </xsl:template>

  <xsl:template match="explanation/account/bullets/bullet">
    <listitem>
      <para><xsl:value-of select="."/></para>
    </listitem>
  </xsl:template>

  <xsl:template match="explanation/definition">
    <listitem>
      <para>
        <xsl:apply-templates/>
      </para>
    </listitem>
  </xsl:template>

  <xsl:template match="definition/intro">
    <xsl:value-of select="."/>
    encoded in 
    <xsl:choose>
      <xsl:when test="../@tabulatedwith">
        <quote><xsl:value-of select="../@encodedin"/></quote>, based on
        <quote><xsl:value-of select="../@tabulatedwith"/></quote>:
      </xsl:when>
      <xsl:otherwise>
        <quote><xsl:value-of select="../@encodedin"/></quote>:
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="definition/after">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="definition/table">
    <blockquote>
      <table tabstyle="definitiontable">
        <xsl:apply-templates/>
      </table>
    </blockquote>
  </xsl:template>

  <xsl:template match="definition/table/tgroup">
    <tgroup cols="{@cols}">
      <xsl:apply-templates/>
    </tgroup>
  </xsl:template>

  <xsl:template match="definition/table/tgroup/thead">
    <thead>
      <xsl:apply-templates/>
    </thead>
  </xsl:template>

  <xsl:template match="row">
    <row>
      <xsl:apply-templates/>
    </row>
  </xsl:template>

  <xsl:template match="thead/row/entry">
    <!--  class="{@class}" -->
    <entry>
      <xsl:apply-templates/>
    </entry>
  </xsl:template>

  <xsl:template match="definition/table/tgroup/tbody">
    <tbody>
      <xsl:apply-templates/>
    </tbody>
  </xsl:template>

  <xsl:template match="definition/table/tgroup/tbody/row/entry">
    <!-- class="{@class} -->
    <entry><xsl:value-of select="."/></entry>
  </xsl:template>

  <xsl:template match="ps_section">
    <sect3>
      <title><xsl:if test="@howmany > 1">Pseudocodes</xsl:if></title>
      <para/>
      <xsl:apply-templates/>
    </sect3>
  </xsl:template>

  <xsl:template match="ps_section/ps">
    <sect4>
      <xsl:attribute name="xreflabel"><xsl:value-of select="@mylink"/></xsl:attribute>
      <title>
        <xsl:choose>
          <xsl:when test="@secttype and @secttype != 'Library'">
            <xsl:value-of select="@secttype"/> pseudocode
          </xsl:when>
          <xsl:when test="@secttype and @secttype = 'Library'">
            Library pseudocode for <xsl:value-of select="@name" /> instructions
          </xsl:when>
          <xsl:otherwise>
            Pseudocode
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="@diagram and ../@howmany > 1" >
          for encodings: <xsl:value-of select="@enclabels"/>
        </xsl:if>
      </title>

      <xsl:if test = "@diagram">
        <para>Symbols used in the pseudocode can be found in this diagram:</para>
      </xsl:if>
      <xsl:apply-templates />
    </sect4>
  </xsl:template>
  
  <xsl:template match="pstext">
    <xsl:if test="not(../@secttype)">
      <para>
        <emphasis><xsl:value-of select="@section"/> pseudocode</emphasis>
      </para>
    </xsl:if>
    <xsl:for-each select="line">
      <programlisting>
        <xsl:if test="@link">
          <xsl:attribute name="xreflabel"><xsl:value-of select="@link"/></xsl:attribute>
        </xsl:if>
        <xsl:value-of select="@indent"/><xsl:apply-templates/>
      </programlisting>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="txt">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="pstext/lineNOT">
    <xsl:value-of select="@indent"/><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="pstext/line/anchor">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="pstext/line/aNOT">
    <xref linkend="{@link}" role="AllText">
      <xsl:attribute name="text"><xsl:value-of select="."/></xsl:attribute>
    </xref>
  </xsl:template>

  <xsl:template match="pstext/line/a">
    <!-- would like something like aNOt above -->
    <xsl:value-of select="."/>
  </xsl:template>

</xsl:stylesheet>

